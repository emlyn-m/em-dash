#include "net/calendar.hpp"

#include "config.hpp"
#include "dispatch.hpp"
#include "log.hpp"
#include "net/cJSON.h"
#include "net/jwt.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

namespace ui {

namespace {

constexpr int MAX_CAL_EVENTS = 20;

Calendar g_calendar;

time_t parse_gcal_datetime(cJSON *obj) {
  struct tm t = {};

  if (cJSON_HasObjectItem(obj, "date")) {
    strptime(cJSON_GetObjectItem(obj, "date")->valuestring, "%Y-%m-%d", &t);
    t.tm_isdst = -1;
    return mktime(&t);
  }
  if (!cJSON_HasObjectItem(obj, "dateTime")) return 0;

  const char *str = cJSON_GetObjectItem(obj, "dateTime")->valuestring;
  strptime(str, "%Y-%m-%dT%H:%M:%S", &t);

  // Apply the trailing timezone offset (+hh:mm / -hh:mm / Z).
  const char *time_part = strchr(str, 'T');
  const char *tz = time_part ? strpbrk(time_part, "+-Z") : nullptr;
  int tz_offset = 0;
  if (tz && (*tz == '+' || *tz == '-')) {
    int hh = 0, mm = 0;
    if (sscanf(tz + 1, "%d:%d", &hh, &mm) >= 1) {
      tz_offset = hh * 3600 + mm * 60;
      if (*tz == '-') tz_offset = -tz_offset;
    }
  }
  return timegm(&t) - tz_offset;
}

// Ensure `token` holds a valid access token, refreshing via JWT if expired.
bool ensure_token(std::string &token, time_t &token_exp) {
  time_t now = time(nullptr);
  if (now <= token_exp && !token.empty()) return true;

  std::string jwt;
  if (!generate_gcal_jwt(get_attr_str("GOOGLE_SERVICE_EMAIL"),
                         get_attr_str("GOOGLE_PRIVKEY"), jwt)) {
    LOG(PRI_ERR, "calendar: jwt generation failed\n");
    return false;
  }

  char cmd[4096];
  snprintf(cmd, sizeof cmd,
           "curl -sX POST -H \"Content-Type: "
           "application/x-www-form-urlencoded\" -d "
           "\"grant_type=urn%%3Aietf%%3Aparams%%3Aoauth%%3Agrant-type%%3Ajwt-"
           "bearer&assertion=%s\" %s",
           jwt.c_str(), get_attr_str("GOOGLE_JWT_REDEEM_URL"));

  FILE *fp = popen(cmd, "r");
  if (!fp) {
    LOG(PRI_ERR, "calendar: token redeem exec failed\n");
    return false;
  }
  char resp[2048] = {0};
  bool read_ok = fgets(resp, sizeof resp, fp) != nullptr;
  pclose(fp);
  if (!read_ok) {
    LOG(PRI_ERR, "calendar: no token response\n");
    return false;
  }

  cJSON *root = cJSON_Parse(resp);
  cJSON *access = cJSON_GetObjectItem(root, "access_token");
  cJSON *expires = cJSON_GetObjectItem(root, "expires_in");
  if (!access || !expires) {
    LOG(PRI_ERR, "calendar: bad token response\n");
    cJSON_Delete(root);
    return false;
  }
  token = access->valuestring;
  token_exp = now + expires->valueint;
  cJSON_Delete(root);
  return true;
}

bool fetch_events(const std::string &token, std::vector<CalEvent> &out) {
  time_t lo_t = time(nullptr);
  time_t hi_t = lo_t + 86400;
  struct tm lo = *gmtime(&lo_t);
  struct tm hi = *gmtime(&hi_t);
  char ts_lo[64], ts_hi[64];
  strftime(ts_lo, sizeof ts_lo, "%Y-%m-%dT00:00:00Z", &lo);
  strftime(ts_hi, sizeof ts_hi, "%Y-%m-%dT00:00:00Z", &hi);

  char url[512];
  snprintf(url, sizeof url, get_attr_str("GOOGLE_EVENTS_URL"),
           get_attr_str("GOOGLE_CALENDAR_ID"), MAX_CAL_EVENTS, ts_lo, ts_hi);

  char cmd[2048];
  snprintf(cmd, sizeof cmd, "curl -sH 'Authorization: Bearer %s' '%s'",
           token.c_str(), url);
  FILE *fp = popen(cmd, "r");
  if (!fp) {
    LOG(PRI_ERR, "calendar: events fetch failed\n");
    return false;
  }
  std::string body;
  char chunk[4096];
  size_t got;
  while ((got = fread(chunk, 1, sizeof chunk, fp)) > 0)
    body.append(chunk, got);
  pclose(fp);

  cJSON *root = cJSON_Parse(body.c_str());
  cJSON *items = cJSON_GetObjectItem(root, "items");
  if (!items) {
    LOG(PRI_ERR, "calendar: no items in response\n");
    cJSON_Delete(root);
    return false;
  }

  out.clear();
  int n = cJSON_GetArraySize(items);
  if (n > MAX_CAL_EVENTS) n = MAX_CAL_EVENTS;
  for (int i = 0; i < n; i++) {
    cJSON *obj = cJSON_GetArrayItem(items, i);
    cJSON *summary = cJSON_GetObjectItem(obj, "summary");
    if (!summary) {
      LOG(PRI_WRN, "calendar: event missing summary, skipping\n");
      continue;
    }
    time_t start = parse_gcal_datetime(cJSON_GetObjectItem(obj, "start"));
    time_t end = parse_gcal_datetime(cJSON_GetObjectItem(obj, "end"));
    out.push_back(CalEvent{summary->valuestring, start ? start : lo_t,
                           end ? end : hi_t});
  }

  cJSON_Delete(root);
  LOG(PRI_INF, "calendar: %zu events\n", out.size());
  return true;
}

void calendar_worker(std::function<void()> on_update) {
  long freq = get_attr_long("CALENDAR_UPDATE_FREQUENCY");
  if (freq <= 0) freq = 600;

  std::string token;
  time_t token_exp = 0;

  for (;;) {
    std::vector<CalEvent> parsed;
    if (ensure_token(token, token_exp) && fetch_events(token, parsed)) {
      post_to_main([parsed = std::move(parsed), on_update]() mutable {
        g_calendar.events = std::move(parsed);
        g_calendar.last_update = time(nullptr);
        if (on_update) on_update();
      });
    }
    std::this_thread::sleep_for(std::chrono::seconds(freq));
  }
}

}  // namespace

void calendar_start(std::function<void()> on_update) {
  std::thread(calendar_worker, std::move(on_update)).detach();
}

const Calendar &calendar_state() { return g_calendar; }

}  // namespace ui
