#include "net/weather.hpp"

#include "config.hpp"
#include "dispatch.hpp"
#include "log.hpp"
#include "net/cJSON.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

namespace ui {

namespace {

// How many upcoming hourly entries to keep in the model.
constexpr int WEATHER_EVENTS = 24 * 7;

Weather g_weather;

time_t parse_time_offset(const char *time_buf) {
  struct tm date = {};
  strptime(time_buf, "%Y-%m-%dT%H:%M", &date);
  date.tm_isdst = -1;
  return mktime(&date);
}

void format_date(time_t when, char *res, size_t n) {
  struct tm *date = localtime(&when);
  strftime(res, n, "%Y-%m-%d", date);
}

// Runs the configured curl command and parses the open-meteo hourly arrays into
// `out`, starting from the entry nearest to now. Returns true on success.
bool fetch_weather(std::vector<WeatherEvent> &out) {
  time_t now = time(nullptr);
  char date_low[16], date_high[16];
  format_date(now, date_low, sizeof date_low);
  format_date(now + 7 * 86400, date_high, sizeof date_high);

  char cmd[1024];
  snprintf(cmd, sizeof cmd, get_attr_str("WEATHER_API_CMD"),
           get_attr_str("WEATHER_LATITUDE"), get_attr_str("WEATHER_LONGITUDE"),
           get_attr_str("WEATHER_TIMEZONE"), date_low, date_high);
  LOG(PRI_INF, "weather: fetching\n");

  FILE *fp = popen(cmd, "r");
  if (!fp) {
    LOG(PRI_ERR, "weather: popen failed\n");
    return false;
  }

  std::string body;
  char chunk[4096];
  size_t got;
  while ((got = fread(chunk, 1, sizeof chunk, fp)) > 0)
    body.append(chunk, got);
  pclose(fp);

  LOG(PRI_DBG, "weather raw: <%s> (%d bytes)\n", body.c_str(), body.size());
  cJSON *root = cJSON_Parse(body.c_str());
  cJSON *hourly = cJSON_GetObjectItem(root, "hourly");
  cJSON *temps = cJSON_GetObjectItem(hourly, "temperature_2m");
  cJSON *rain = cJSON_GetObjectItem(hourly, "precipitation_probability");
  cJSON *codes = cJSON_GetObjectItem(hourly, "weather_code");
  cJSON *times = cJSON_GetObjectItem(hourly, "time");
  if (!(root && hourly && temps && rain && codes && times)) {
    LOG(PRI_ERR, "weather: parse failed\n");
    cJSON_Delete(root);
    return false;
  }

  int offset = 0;
  int count = cJSON_GetArraySize(times);

  out.clear();
  for (int i = 0; i < WEATHER_EVENTS && offset < count; i++, offset++) {
    WeatherEvent ev;
    ev.time = parse_time_offset(cJSON_GetArrayItem(times, offset)->valuestring);
    ev.temp_c = cJSON_GetArrayItem(temps, offset)->valuedouble;
    ev.rain_prob = cJSON_GetArrayItem(rain, offset)->valuedouble;
    ev.wmo_code = cJSON_GetArrayItem(codes, offset)->valueint;
    out.push_back(ev);
  }

  cJSON_Delete(root);
  LOG(PRI_INF, "weather: %zu events\n", out.size());
  return true;
}

void weather_worker(std::function<void()> on_update) {
  long freq = get_attr_long("WEATHER_UPDATE_FREQUENCY");
  if (freq <= 0)
    freq = 3600;

  for (;;) {
    std::vector<WeatherEvent> parsed;
    if (fetch_weather(parsed)) {
      post_to_main([parsed = std::move(parsed), on_update]() mutable {
        g_weather.events = std::move(parsed);
        g_weather.last_update = time(nullptr);
        if (on_update)
          on_update();
      });
    }
    std::this_thread::sleep_for(std::chrono::seconds(freq));
  }
}

} // namespace

void weather_start(std::function<void()> on_update) {
  std::thread(weather_worker, std::move(on_update)).detach();
}

const Weather &weather_state() { return g_weather; }

} // namespace ui
