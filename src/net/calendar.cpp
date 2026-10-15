#include "net/calendar.hpp"

#include "config.hpp"
#include "dispatch.hpp"
#include "log.hpp"
#include "net/cJSON.h"
#include "net/jwt.hpp"

#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

namespace ui {

namespace {

constexpr int MAX_CAL_EVENTS = 20;
constexpr size_t MAX_TODO_TASKS = 20;
constexpr int MAX_TODO_FETCH = 250;

Calendar g_calendar;
Todos g_todo;

time_t parse_gcal_datetime(cJSON *obj) {
  struct tm t = {};

  if (cJSON_HasObjectItem(obj, "date")) {
    strptime(cJSON_GetObjectItem(obj, "date")->valuestring, "%Y-%m-%d", &t);
    t.tm_isdst = -1;
    return mktime(&t);
  }
  if (!cJSON_HasObjectItem(obj, "dateTime"))
    return 0;

  const char *str = cJSON_GetObjectItem(obj, "dateTime")->valuestring;
  strptime(str, "%Y-%m-%dT%H:%M:%S", &t);

  const char *time_part = strchr(str, 'T');
  const char *tz = time_part ? strpbrk(time_part, "+-Z") : nullptr;
  int tz_offset = 0;
  if (tz && (*tz == '+' || *tz == '-')) {
    int hh = 0, mm = 0;
    if (sscanf(tz + 1, "%d:%d", &hh, &mm) >= 1) {
      tz_offset = hh * 3600 + mm * 60;
      if (*tz == '-')
        tz_offset = -tz_offset;
    }
  }
  return timegm(&t) - tz_offset;
}

bool ensure_token(std::string &token, time_t &token_exp) {
  time_t now = time(nullptr);
  if (now <= token_exp && !token.empty())
    return true;

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

// Query internal - use both RFC dates and raw time_t
struct Window {
  char ts_lo[64];
  char ts_hi[64];
  time_t lo_t;
  time_t hi_t;
};

// now - +1 week
Window event_window() {
  Window w;
  w.lo_t = time(nullptr);
  w.hi_t = w.lo_t + 7 * 86400;
  struct tm lo = *gmtime(&w.lo_t);
  struct tm hi = *gmtime(&w.hi_t);
  strftime(w.ts_lo, sizeof w.ts_lo, "%Y-%m-%dT00:00:00Z", &lo);
  strftime(w.ts_hi, sizeof w.ts_hi, "%Y-%m-%dT00:00:00Z", &hi);
  return w;
}

Window todo_window() {
  long days = get_attr_long("TODO_HISTORY_DAYS");
  if (days <= 0)
    days = 90;

  Window w;
  w.lo_t = time(nullptr) - days * 86400;
  w.hi_t = INT_MAX;

  struct tm lo = *gmtime(&w.lo_t);
  struct tm hi = *gmtime(&w.hi_t);
  strftime(w.ts_lo, sizeof w.ts_lo, "%Y-%m-%dT%H:%M:%SZ", &lo);
  strftime(w.ts_hi, sizeof w.ts_hi, "%Y-%m-%dT%H:%M:%SZ", &hi);
  return w;
}

time_t local_midnight() {
  time_t now = time(nullptr);
  struct tm d = *localtime(&now);
  d.tm_hour = d.tm_min = d.tm_sec = 0;
  d.tm_isdst = -1;
  return mktime(&d);
}

std::string json_escape(const std::string &s) {
  std::string out;
  for (char c : s) {
    if (c == '"' || c == '\\')
      out += '\\';
    if ((unsigned char)c >= 0x20)
      out += c;
  }
  return out;
}

std::string shell_quote(const std::string &s) {
  std::string out = "'";
  for (char c : s) {
    if (c == '\'')
      out += "'\\''";
    else
      out += c;
  }
  return out + "'";
}

cJSON *fetch_events_json(const std::string &token, const char *cal_id,
                         const Window &win, int max_results) {
  if (!cal_id || !*cal_id) {
    LOG(PRI_WRN, "calendar: no calendar id configured\n");
    return NULL;
  }

  char url[512];
  snprintf(url, sizeof url, get_attr_str("GOOGLE_EVENTS_URL"), cal_id,
           max_results, win.ts_lo, win.ts_hi);

  char cmd[2048];
  snprintf(cmd, sizeof cmd, "curl -sH 'Authorization: Bearer %s' '%s'",
           token.c_str(), url);
  FILE *fp = popen(cmd, "r");
  if (!fp) {
    return NULL;
  }
  std::string body;
  char chunk[4096];
  size_t got;
  while ((got = fread(chunk, 1, sizeof chunk, fp)) > 0)
    body.append(chunk, got);
  pclose(fp);

  cJSON *root = cJSON_Parse(body.c_str());
  return root;
}

bool fetch_events(const std::string &token, const char *cal_id,
                  const Window &win, std::vector<CalEvent> &out) {
  cJSON *root = fetch_events_json(token, cal_id, win, MAX_CAL_EVENTS);
  if (!root) {
    LOG(PRI_ERR, "calendar: events fetch failed\n");
    return false;
  }

  cJSON *items = cJSON_GetObjectItem(root, "items");
  if (!items) {
    LOG(PRI_ERR, "calendar: no items in response\n");
    cJSON_Delete(root);
    return false;
  }

  out.clear();
  int n = cJSON_GetArraySize(items);
  if (n > MAX_CAL_EVENTS)
    n = MAX_CAL_EVENTS;
  for (int i = 0; i < n; i++) {
    cJSON *obj = cJSON_GetArrayItem(items, i);
    cJSON *summary = cJSON_GetObjectItem(obj, "summary");
    if (!summary) {
      LOG(PRI_WRN, "calendar: event missing summary, skipping\n");
      continue;
    }
    cJSON *id = cJSON_GetObjectItem(obj, "id");
    time_t start = parse_gcal_datetime(cJSON_GetObjectItem(obj, "start"));
    time_t end = parse_gcal_datetime(cJSON_GetObjectItem(obj, "end"));

    out.push_back(CalEvent{id ? id->valuestring : "", summary->valuestring,
                           start ? start : win.lo_t, end ? end : win.hi_t});
  }

  cJSON_Delete(root);
  LOG(PRI_INF, "calendar: %zu events from %s\n", out.size(), cal_id);
  return true;
}

bool fetch_tasks(const std::string &token, const char *cal_id,
                 const Window &win, std::vector<TodoTask> &out,
                 std::vector<int> &out_complete) {
  cJSON *root = fetch_events_json(token, cal_id, win, MAX_TODO_FETCH);
  if (!root) {
    LOG(PRI_ERR, "todo: events fetch failed\n");
    return false;
  }

  cJSON *items = cJSON_GetObjectItem(root, "items");
  if (!items) {
    LOG(PRI_WRN, "todo: no items in response\n");
    cJSON_Delete(root);
    return false;
  }

  out.clear();
  out_complete.clear();

  const time_t today = local_midnight();
  int n = cJSON_GetArraySize(items);
  for (int i = 0; i < n && out.size() < MAX_TODO_TASKS; i++) {
    cJSON *obj = cJSON_GetArrayItem(items, i);

    cJSON *summary = cJSON_GetObjectItem(obj, "summary");
    if (!summary) {
      LOG(PRI_WRN, "todo: event missing summary, skipping\n");
      continue;
    }
    cJSON *id = cJSON_GetObjectItem(obj, "id");
    time_t start = parse_gcal_datetime(cJSON_GetObjectItem(obj, "start"));

    const char *title = summary->valuestring;
    bool completed = false, needsformat = false;
    if (!strncmp(title, "[x]", 3)) {
      completed = true;
      title += 4;
    } else if (!strncmp(title, "[ ]", 3)) {
      title += 4;
    } else {
      needsformat = true;
    }

    if (completed && start < today)
      continue;

    char title_c[128] = {0};
    snprintf(title_c, sizeof title_c, "%s", title);

    out.push_back(TodoTask{id ? id->valuestring : "", title_c, start, completed,
                           needsformat});
    out_complete.push_back(-1);
  }

  cJSON_Delete(root);
  LOG(PRI_INF, "todo: %zu events from %s\n", out.size(), cal_id);

  int now = time(nullptr);
  std::sort(out.begin(), out.end(),
            [now](const TodoTask &a, const TodoTask &b) {
              return abs((int)a.time - now) < abs((int)b.time - now);
            });

  return true;
}

void update_tasks(const std::string &token, const char *cal_id) {
  const std::vector<TodoTask> tasks = g_todo.tasks;
  const std::vector<int> complete = g_todo.complete;

  for (size_t i = 0; i < tasks.size(); i++) {
    if ((!tasks[i].net_needsformat) &&
        ((complete[i] < 0) || (tasks[i].net_complete == complete[i]))) {
      continue;
    }
    bool task_complete = complete[i] >= 0 ? complete[i] : tasks[i].net_complete;

    char url[512];
    snprintf(url, sizeof url,
             "https://www.googleapis.com/calendar/v3/calendars/%s/events/%s",
             cal_id, tasks[i].id.c_str());

    std::string payload = std::string("{\"summary\": \"[") +
                          (task_complete ? 'x' : ' ') + "] " +
                          json_escape(tasks[i].title) + "\"}";

    std::string cmd = "curl -sX PATCH -H 'Content-Type: application/json' "
                      "-H 'Authorization: Bearer " +
                      token + "' '" + url + "' -d " + shell_quote(payload);

    FILE *fp = popen(cmd.c_str(), "r");
    if (!fp) {
      LOG(PRI_ERR, "todo: update failed\n");
      return;
    }

    std::string body;
    char chunk[4096];
    size_t got;
    while ((got = fread(chunk, 1, sizeof chunk, fp)) > 0)
      body.append(chunk, got);
    pclose(fp);

    if (body.find("\"error\"") != std::string::npos)
      LOG(PRI_WRN, "todo: update of %s rejected\n", tasks[i].id.c_str());
  }

  LOG(PRI_DBG, "todo: update complete\n");
}

void calendar_worker(std::function<void()> cal_update,
                     std::function<void()> todo_update) {
  long freq = get_attr_long("CALENDAR_UPDATE_FREQUENCY");
  if (freq <= 0)
    freq = 600;

  std::string token;
  time_t token_exp = 0;

  for (;;) {
    std::vector<CalEvent> events;
    std::vector<TodoTask> tasks;
    std::vector<int> complete;
    bool got_events = false, got_todos = false;

    if (ensure_token(token, token_exp)) {
      // update first as g_todo.completed is overwritten - i'd love to not do
      // this (quicker launch but ughh - fix later i spose)
      update_tasks(token, get_attr_str("GOOGLE_TODO_CALENDAR_ID"));
      got_events = fetch_events(token, get_attr_str("GOOGLE_CALENDAR_ID"),
                                event_window(), events);
      got_todos = fetch_tasks(token, get_attr_str("GOOGLE_TODO_CALENDAR_ID"),
                              todo_window(), tasks, complete);
    }

    if (got_events) {
      post_to_main([events = std::move(events), cal_update]() mutable {
        g_calendar.events = std::move(events);
        g_calendar.last_update = time(nullptr);
        if (cal_update)
          cal_update();
      });
    }

    if (got_todos) {
      post_to_main([tasks = std::move(tasks), complete = std::move(complete),
                    todo_update]() mutable {
        g_todo.tasks = std::move(tasks);
        g_todo.complete = std::move(complete);
        g_todo.last_update = time(nullptr);

        if (todo_update) {
          todo_update();
        }
      });
    }

    std::this_thread::sleep_for(std::chrono::seconds(freq));
  }
}

} // namespace

void calendar_start(std::function<void()> cal_update,
                    std::function<void()> todo_update) {
  std::thread(calendar_worker, std::move(cal_update), std::move(todo_update))
      .detach();
}

void todo_mark(int local_id) {
  if (local_id >= 0 && local_id < (int)g_todo.tasks.size()) {
    g_todo.complete[local_id] = g_todo.complete[local_id] >= 0
                                    ? (int)!g_todo.complete[local_id]
                                    : (int)!g_todo.tasks[local_id].net_complete;
  }
}

const Calendar &calendar_state() { return g_calendar; }
const Todos &todo_state() { return g_todo; }

} // namespace ui
