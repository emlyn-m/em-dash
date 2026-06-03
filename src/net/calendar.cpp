#include "src/config.hpp"
#include "src/log.hpp"
#include "src/net/cJSON.h"
#include "src/net/net.hpp"
#include "src/widgets/widgets.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <pthread.h>

time_t parse_gcal_datetime(cJSON *obj) {
  struct tm return_time;
  memset(&return_time, 0, sizeof(struct tm));

  if (cJSON_HasObjectItem(obj, "date")) {
    // todo - fuck surely this doesnt need timezones too...
    strptime(cJSON_GetObjectItem(obj, "date")->valuestring, "%Y-%m-%d",
             &return_time);
    return_time.tm_isdst = -1;
    return mktime(&return_time);
  } else if (cJSON_HasObjectItem(obj, "dateTime")) {
    const char *datetime_str =
        cJSON_GetObjectItem(obj, "dateTime")->valuestring;
    strptime(datetime_str, "%Y-%m-%dT%H:%M:%S", &return_time);

    const char *time_part = strchr(datetime_str, 'T');
    const char *tz_pos = NULL;
    if (time_part) {
      tz_pos = strpbrk(time_part, "+-Z");
    }

    int tz_offset_seconds = 0;
    if (tz_pos && (*tz_pos == '+' || *tz_pos == '-')) {
      int tz_hours = 0, tz_mins = 0;
      if (sscanf(tz_pos + 1, "%d:%d", &tz_hours, &tz_mins) >= 1) {
        tz_offset_seconds = (tz_hours * 3600) + (tz_mins * 60);
        if (*tz_pos == '-') {
          tz_offset_seconds = -tz_offset_seconds;
        }
      }
    }

    time_t result = timegm(&return_time) - tz_offset_seconds;
    return result;
  }

  return 0;
}

void *update_events_async(void *data_vp) {
  const uint32_t TOKEN_BUFSIZE = 2048;
  unsigned long ctime = time(NULL);
  calendar_t *cal = (calendar_t *)data_vp;

  if (ctime > cal->token_exp) {

    fflush(stdout);

    if (!cal->token_buf) {
      cal->token_buf = (char *)malloc(TOKEN_BUFSIZE * sizeof(char));
      memset(cal->token_buf, 0, TOKEN_BUFSIZE);
    }

    int token_result = generate_gcal_jwt(
        (char *)get_attr_str("GOOGLE_SERVICE_EMAIL"),
        (char *)get_attr_str("GOOGLE_PRIVKEY"), TOKEN_BUFSIZE, cal->token_buf);
    if (token_result) {
      LOG(PRI_ERR, "failed to generate jwt\n");
      fflush(stdout);
      return NULL;
    } // error!!

    const uint32_t token_redeem_bufsize = 2048;
    char *token_redeem_payload =
        (char *)malloc(token_redeem_bufsize * sizeof(char));
    snprintf(token_redeem_payload, token_redeem_bufsize,
             "curl -sX POST -H \"Content-Type: "
             "application/x-www-form-urlencoded\" -d "
             "\"grant_type=urn%%3Aietf%%3Aparams%%3Aoauth%%3Agrant-type%%3Ajwt-"
             "bearer&assertion=%s\" %s",
             cal->token_buf, get_attr_str("GOOGLE_JWT_REDEEM_URL"));
    FILE *token_redeem_fp = popen(token_redeem_payload, "r");
    free(token_redeem_payload);
    if (!token_redeem_fp) { /* failed to exec -  yikes! */
      LOG(PRI_ERR, "failed to exec token redeem\n");
      fflush(stdout);
      return NULL;
    }

    const uint32_t token_resp_bufsize = 2048;
    char *token_resp_buf = (char *)malloc(token_resp_bufsize * sizeof(char));
    if (!fgets(token_resp_buf, token_resp_bufsize, token_redeem_fp)) {
      LOG(PRI_ERR, "failed to read token response\n");
      fflush(stdout);
      pclose(token_redeem_fp);
      free(token_resp_buf);
      return NULL;
    }
    int token_redeem_status = pclose(token_redeem_fp);
    if (token_redeem_status) {
      LOG(PRI_ERR, "token redeem exit_code=%d\n", token_redeem_status);
      fflush(stdout);
      return NULL;
    }

    cJSON *token_resp_j = cJSON_Parse(token_resp_buf);
    free(token_resp_buf);
    if (!token_resp_j) {
      LOG(PRI_ERR, "failed to parse token response\n");
      fflush(stdout);
      return NULL;
    }

    cJSON *expires_in_j = cJSON_GetObjectItem(token_resp_j, "expires_in");
    cJSON *access_token_j = cJSON_GetObjectItem(token_resp_j, "access_token");
    if (!expires_in_j || !access_token_j) {
      LOG(PRI_ERR, "missing fields in token response\n");
      fflush(stdout);
      cJSON_Delete(token_resp_j);
      return NULL;
    }

    cal->token_exp = ctime + expires_in_j->valueint;
    strcpy(cal->token_buf, access_token_j->valuestring);

    cJSON_Delete(token_resp_j);
  }

  time_t tlo_t;
  time_t thi_t;
  time(&tlo_t);
  time(&thi_t);
  struct tm tlo = *gmtime(&tlo_t);
  thi_t += 86400; // 1 day
  struct tm thi = *gmtime(&thi_t);
  char timestamp_lo[64];
  char timestamp_hi[64];
  strftime(timestamp_lo, 64, "%Y-%m-%dT00:00:00Z", &tlo);
  strftime(timestamp_hi, 64, "%Y-%m-%dT00:00:00Z", &thi);

  char events_req_url[512];
  snprintf(events_req_url, 512, get_attr_str("GOOGLE_EVENTS_URL"),
           get_attr_str("GOOGLE_CALENDAR_ID"), MAX_CAL_EVENTS, timestamp_lo,
           timestamp_hi);

  char events_req_cmdbuf[2048];
  snprintf(events_req_cmdbuf, 2048, "curl -sH 'Authorization: Bearer %s' '%s'",
           cal->token_buf, events_req_url);
  FILE *events_req_fp = popen(events_req_cmdbuf, "r");
  if (!events_req_fp) {
    LOG(PRI_ERR, "failed to exec events fetch\n");
    return NULL;
  }

  const uint32_t EVENTS_BUF_SIZE =
      1000000; // todo: wow i really dont even know if this'll be long enough;
  char events_buf[EVENTS_BUF_SIZE];
  memset(events_buf, 0, EVENTS_BUF_SIZE);
  fread(events_buf, sizeof(char), EVENTS_BUF_SIZE, events_req_fp);
  int events_req_exitcode = pclose(events_req_fp);
  if (events_req_exitcode) {
    LOG(PRI_ERR, "events_req exitcode=%d\n", events_req_exitcode);
    return NULL;
  }

  cJSON *events_j = cJSON_Parse(events_buf);
  if (!events_j) {
    LOG(PRI_ERR, "failed to parse events response\n");
    fflush(stdout);
    return NULL;
  }
  cJSON *event_lst = cJSON_GetObjectItem(events_j, "items");
  if (!event_lst) {
    LOG(PRI_ERR, "missing items in events response\n");
    fflush(stdout);
    cJSON_Delete(events_j);
    return NULL;
  }

  for (int i = 0; i < MIN(cJSON_GetArraySize(event_lst), MAX_CAL_EVENTS); i++) {
    cJSON *event_obj = cJSON_GetArrayItem(event_lst, i);

    if (!cal->events[i]) {
      cal->events[i] = (cal_event_t *)malloc(sizeof(cal_event_t));
      cal->events[i]->title = NULL;
    }
    cal->events[i]->id = i;
    if (cal->events[i]->title != NULL) {
      free(cal->events[i]->title);
    } // this seems to be crashing...

    cJSON *summary_j = cJSON_GetObjectItem(event_obj, "summary");
    if (!summary_j) {
      LOG(PRI_WRN, "event missing summary, skipping\n");
      fflush(stdout);
      continue;
    }
    char *event_title_tmp = summary_j->valuestring;
    cal->events[i]->title = (char *)malloc(strlen(event_title_tmp) + 1);
    memset(cal->events[i]->title, 0, strlen(event_title_tmp) + 1);
    strcpy(cal->events[i]->title, event_title_tmp);

    cal->events[i]->start_time =
        parse_gcal_datetime(cJSON_GetObjectItem(event_obj, "start"));
    if (!cal->events[i]->start_time) {
      cal->events[i]->start_time = tlo_t;
    } // surely neither of these should happen BUT just in case :)

    cal->events[i]->end_time = parse_gcal_datetime(cJSON_GetObjectItem(
        event_obj, "end")); // we need way better logic for handling the 'date'
                            // format on this
    if (!cal->events[i]->end_time) {
      cal->events[i]->end_time = thi_t;
    } // surely neither of these should happen BUT just in case :)
  }

  cal->num_events = MIN(cJSON_GetArraySize(event_lst), MAX_CAL_EVENTS);
  cal->last_updated = time(NULL);
  cJSON_Delete(events_j);
  return NULL;
}

gboolean update_events(gpointer *calendar_gp) {
  calendar_t *cal = (calendar_t *)calendar_gp;
  long ctime = time(NULL);

  if ((long)(cal->last_updated + cal->update_frequency) > (long)ctime) {
    return TRUE;
  }
  cal->last_updated = ctime;
  fflush(stdout);

  pthread_t thread_id;
  pthread_create(&thread_id, NULL, update_events_async, calendar_gp);
  pthread_detach(thread_id);

  return TRUE;
}
