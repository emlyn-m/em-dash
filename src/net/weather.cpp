#include "glib.h"
#include "../secrets.h"
#include "gtk/gtk.h"
#include "src/net/cJSON.h"
#include "src/widgets/widgets.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#define WEATHER_API_CMD "curl -s 'https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s&hourly=temperature_2m,precipitation_probability&start_date=%s&end_date=%s&timezone=%s'"

time_t parse_time_offset(char* time_buf) {
    struct tm date = {0};
    strptime(time_buf, "%Y-%m-%dT%H:%M", &date);
    date.tm_isdst = -1;
    
    return mktime(&date);
}
void generate_date(time_t date_begin, char* res) {
    struct tm* date = localtime(&date_begin);
    strftime(res, 15, (char*) "%Y-%m-%d", date);
    
}

gboolean update_weather(gpointer* data) {
    
    weather_t* weather_data = (weather_t*) data;
    
    time_t ctime;
    if ( weather_data->last_update + weather_data->update_freq > (ctime = time(NULL)) ) {
        return TRUE;
    }
    
    printf("beginning weather update\n");
    
    char date_low[15];
    char date_high[15];
    generate_date(((ctime - 86400) / 86400) * 86400, date_low);
    generate_date(((ctime + 86400) / 86400) * 86400, date_high);
    
    
    char* weather_req_cmdbuf = (char*) malloc(1024 * sizeof(char));
    snprintf(weather_req_cmdbuf, 1024, WEATHER_API_CMD, WEATHER_LATITUDE, WEATHER_LONGITUDE, date_low, date_high, WEATHER_TIMEZONE);
    printf("fetching apiurl <%s>\n", weather_req_cmdbuf);
    FILE* weather_fp = popen(weather_req_cmdbuf, "r");
    free(weather_req_cmdbuf);
    if (!weather_fp) {
        fprintf(stderr, "failed to fetch weather\n");
        return TRUE;
    }
    printf("fetched weather\n");
    
    
    const size_t WEATHER_SIZE = 100000;
    char* weather_buf = (char*) malloc(WEATHER_SIZE * sizeof(char));
    if ( fread(weather_buf, sizeof(char), WEATHER_SIZE, weather_fp) == WEATHER_SIZE ) {
        printf("err: read filled buffer of weather_req - end: <%s>\n", weather_buf + WEATHER_SIZE - 10);
        free(weather_buf);
        return TRUE;
    };
    cJSON* weather = cJSON_Parse(weather_buf);
    cJSON* hourly = cJSON_GetObjectItem(weather, "hourly");
    cJSON* temps = cJSON_GetObjectItem(hourly, "temperature_2m");
    cJSON* rain_probs = cJSON_GetObjectItem(hourly, "precipitation_probability");
    cJSON* weather_times = cJSON_GetObjectItem(hourly, "time");
    
    if (!(weather || hourly || temps || rain_probs || weather_times)) {
        fprintf(stderr, "json parsing not working: <%s>\n", weather_buf);
        fflush(stderr);
        return TRUE;
    }
        
    int time_offset = 0;
    
    
    while ( parse_time_offset(cJSON_GetArrayItem(weather_times, time_offset+1)->valuestring) < ctime ) { time_offset++; }
    int event_idx = 0;
    while (event_idx < weather_data->num_weather_events) {
        printf("[%d]... ", event_idx);
        
        weather_data->events[event_idx]->time = parse_time_offset(cJSON_GetArrayItem(weather_times, time_offset)->valuestring);
        weather_data->events[event_idx]->temp_c = cJSON_GetArrayItem(temps, time_offset)->valuedouble;
        weather_data->events[event_idx]->rain_prob = cJSON_GetArrayItem(rain_probs, time_offset)->valuedouble;
        
        printf("ts=%s    t=%ld    T=%lf*C    P=%lf\n", cJSON_GetArrayItem(weather_times, time_offset)->valuestring, weather_data->events[event_idx]->time, weather_data->events[event_idx]->temp_c, weather_data->events[event_idx]->rain_prob / 100.0);
        
        time_offset++;
        event_idx++;
    }    
    
    cJSON_free(weather);
    free(weather_buf);
    
    weather_data->last_update = ctime;
    printf("weather update complete\n");
    
    return TRUE;
}
