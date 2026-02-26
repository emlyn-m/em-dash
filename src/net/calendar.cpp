#include "src/log.hpp"
#include "src/net/net.hpp"
#include "src/net/cJSON.h"
#include "src/widgets/widgets.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <pthread.h>


time_t parse_gcal_datetime(cJSON* obj) {
	struct tm return_time;
	memset(&return_time, 0, sizeof(struct tm));

	if (cJSON_HasObjectItem(obj, "date")) {
	    // todo - fuck surely this doesnt need timezones too...
	    strptime(cJSON_GetObjectItem(obj, "date")->valuestring, "%Y-%m-%d", &return_time);
	    return_time.tm_isdst = -1;
	    return mktime(&return_time);
	}
	else if (cJSON_HasObjectItem(obj, "dateTime")) {
	    const char* datetime_str = cJSON_GetObjectItem(obj, "dateTime")->valuestring;
	    strptime(datetime_str, "%Y-%m-%dT%H:%M:%S", &return_time);

	    const char* time_part = strchr(datetime_str, 'T');
	    const char* tz_pos = NULL;
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

void* update_events_async(void* data_vp) {
    const uint32_t TOKEN_BUFSIZE = 2048;
   	unsigned long ctime = time(NULL);
    calendar_t* cal = (calendar_t*) data_vp;
    
   	if (ctime > cal->token_exp) {
   
	    LOG(PRI_INF, "expired token, regenerating...\n");
	    fflush(stdout);
   
	    if (!cal->token_buf) {
	        cal->token_buf = (char*) malloc(TOKEN_BUFSIZE * sizeof(char));
	        memset(cal->token_buf, 0, TOKEN_BUFSIZE);
	    }
   
	    int token_result = generate_gcal_jwt((char*) getenv("GOOGLE_SERVICE_EMAIL"), (char*) getenv("GOOGLE_PRIVKEY"), TOKEN_BUFSIZE, cal->token_buf);
	    if (token_result) {
	        LOG(PRI_ERR, "failed to generate jwt\n"); fflush(stdout);
	        _exit(0);
	    } // error!!
   
	    LOG(PRI_INF, "successfully generated jwt\n"); fflush(stdout);
   
	    const uint32_t token_redeem_bufsize = 2048;
	    char* token_redeem_payload = (char*) malloc(token_redeem_bufsize * sizeof(char));
	    snprintf(
	        token_redeem_payload, token_redeem_bufsize,
	        "curl -sX POST -H \"Content-Type: application/x-www-form-urlencoded\" -d \"grant_type=urn%%3Aietf%%3Aparams%%3Aoauth%%3Agrant-type%%3Ajwt-bearer&assertion=%s\" %s",
	        cal->token_buf, getenv("GOOGLE_JWT_REDEEM_URL")
	    );
	    FILE* token_redeem_fp = popen(token_redeem_payload, "r");
	    free(token_redeem_payload);
	    if (!token_redeem_fp) { /* failed to exec -  yikes! */
	        LOG(PRI_ERR, "failed to exec token redeem\n"); fflush(stdout);
	        _exit(0);
	    }
	    LOG(PRI_INF, "redeemed google oauth token\n"); fflush(stdout);
   
	    const uint32_t token_resp_bufsize = 2048;
	    char* token_resp_buf = (char*) malloc(token_resp_bufsize * sizeof(char));
	    fgets(token_resp_buf, token_resp_bufsize, token_redeem_fp);
   
   
	    cJSON* token_resp_j = cJSON_Parse(token_resp_buf);
	    free(token_resp_buf);
   
	    int token_expiry = cJSON_GetObjectItem(token_resp_j, "expires_in")->valueint;
	    char* token_value = cJSON_GetObjectItem(token_resp_j, "access_token")->valuestring;
   
	    cal->token_exp = ctime + token_expiry;
	    strcpy(cal->token_buf, token_value);
   
	    cJSON_free(token_resp_j);
	}
   
	LOG(PRI_INF, "existing valid token found\n"); fflush(stdout);
   
	time_t tlo_t;
	time_t thi_t;
	time(&tlo_t); time(&thi_t);
	struct tm tlo = *gmtime(&tlo_t);
	thi_t += 86400;  // 1 day
	struct tm thi = *gmtime(&thi_t);
	char timestamp_lo[64];
	char timestamp_hi[64];
	strftime(timestamp_lo, 64, "%Y-%m-%dT00:00:00Z", &tlo);
	strftime(timestamp_hi, 64, "%Y-%m-%dT00:00:00Z", &thi);
   
	char events_req_url[512];
	snprintf(
	    events_req_url, 512,
	    getenv("GOOGLE_EVENTS_URL"), getenv("GOOGLE_CALENDAR_ID"), MAX_CAL_EVENTS,
	    timestamp_lo, timestamp_hi
	);
   
	char events_req_cmdbuf[2048];
	snprintf(events_req_cmdbuf, 2048, "curl -sH 'Authorization: Bearer %s' '%s'", cal->token_buf, events_req_url);
	FILE* events_req_fp = popen(events_req_cmdbuf, "r");
	if (!events_req_fp) { LOG(PRI_ERR, "failed to exec events fetch\n"); _exit(0); }
   
	const uint32_t EVENTS_BUF_SIZE = 1000000; // todo: wow i really dont even know if this'll be long enough;
	char events_buf[EVENTS_BUF_SIZE];
	memset(events_buf, 0, EVENTS_BUF_SIZE);
	fread(events_buf, sizeof(char), EVENTS_BUF_SIZE, events_req_fp);
   
	cJSON* event_lst = cJSON_GetObjectItem(cJSON_Parse(events_buf), "items");
   
	for (int i=0; i < cJSON_GetArraySize(event_lst); i++) {
	    cJSON* event_obj = cJSON_GetArrayItem(event_lst, i);
   
	    if (!cal->events[i]) { cal->events[i] = (cal_event_t*) malloc(sizeof(cal_event_t)); cal->events[i]->title = NULL; }
	    cal->events[i]->id = i;
	    if (cal->events[i]->title != NULL) { free(cal->events[i]->title); }  // this seems to be crashing...
   
	    char* event_title_tmp = cJSON_GetObjectItem(event_obj, "summary")->valuestring;
	    cal->events[i]->title = (char*) malloc(strlen(event_title_tmp) + 1); memset(cal->events[i]->title, 0, strlen(event_title_tmp)+1);
	    strcpy(cal->events[i]->title, event_title_tmp);
   
	    cal->events[i]->start_time = parse_gcal_datetime(cJSON_GetObjectItem(event_obj, "start"));
	    if (!cal->events[i]->start_time) { cal->events[i]->start_time = tlo_t; }  // surely neither of these should happen BUT just in case :)
   
	    cal->events[i]->end_time = parse_gcal_datetime(cJSON_GetObjectItem(event_obj, "end"));  // we need way better logic for handling the 'date' format on this
	    if (!cal->events[i]->end_time) { cal->events[i]->end_time = thi_t; }  // surely neither of these should happen BUT just in case :)
   
	    LOG(PRI_INF, "found event %s (%ld - %ld)\n", cal->events[i]->title, cal->events[i]->start_time, cal->events[i]->end_time); fflush(stdout);
	}
   
	cal->num_events = cJSON_GetArraySize(event_lst);
	cal->last_updated = time(NULL);
	return NULL;
}

gboolean update_events(gpointer* calendar_gp) {
	calendar_t* cal = (calendar_t*) calendar_gp;
	long ctime = time(NULL);

	if ((long) (cal->last_updated + cal->update_frequency) > (long) ctime) {
	    return TRUE;
	}
	cal->last_updated = ctime;

	LOG(PRI_INF, "executing calendar update...\n"); fflush(stdout);
	fflush(stdout);
	
	pthread_t thread_id;
	pthread_create(&thread_id, NULL, update_events_async, calendar_gp);


	LOG(PRI_INF, "calendar update complete\n"); fflush(stdout);
	return TRUE;
}
