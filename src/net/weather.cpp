#include "glib.h"
#include "src/net/cJSON.h"
#include "src/widgets/widgets.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <pthread.h>


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

void* update_weather_async(void* data_vp) {
    weather_t* weather_data = (weather_t*) data_vp;
   	time_t ctime = time(NULL);
    
   	char date_low[15];
	char date_high[15];
	generate_date(((ctime - 86400) / 86400) * 86400, date_low);
	generate_date(((ctime + 86400) / 86400) * 86400, date_high);
   
   
	char* weather_req_cmdbuf = (char*) malloc(1024 * sizeof(char));
	snprintf(weather_req_cmdbuf, 1024, getenv("WEATHER_API_CMD"), getenv("WEATHER_LATITUDE"), getenv("WEATHER_LONGITUDE"), getenv("WEATHER_TIMEZONE"), date_low, date_high);
	printf("\x1b[38;5;139m\x1b[1mINFO:\x1b[0m using fetch command \"%s\"\n", weather_req_cmdbuf); fflush(stdout);
	FILE* weather_fp = popen(weather_req_cmdbuf, "r");
	free(weather_req_cmdbuf);
	if (!weather_fp) {
	    fprintf(stderr, "\x1b[38;5;139m\x1b[1mERR:\x1b[0m failed to fetch weather\n"); fflush(stderr);
	    return NULL;
	}
	printf("\x1b[38;5;139m\x1b[1mINFO:\x1b[0m fetched weather\n"); fflush(stdout);
   
   
	const size_t WEATHER_SIZE = 100000;
	char* weather_buf = (char*) malloc(WEATHER_SIZE * sizeof(char)); memset(weather_buf, 0, WEATHER_SIZE);
	if ( fread(weather_buf, sizeof(char), WEATHER_SIZE, weather_fp) == WEATHER_SIZE ) {
	    printf("\x1b[38;5;139m\x1b[1mERR:\x1b[0m read filled buffer of weather_req - end: <%s>\n", weather_buf + WEATHER_SIZE - 10); fflush(stdout);
	    free(weather_buf);
	    return NULL;
	};
	cJSON* weather = cJSON_Parse(weather_buf);
	cJSON* hourly = cJSON_GetObjectItem(weather, "hourly");
	cJSON* temps = cJSON_GetObjectItem(hourly, "temperature_2m");
	cJSON* rain_probs = cJSON_GetObjectItem(hourly, "precipitation_probability");
	cJSON* wmo_codes = cJSON_GetObjectItem(hourly, "weather_code");
	cJSON* weather_times = cJSON_GetObjectItem(hourly, "time");
   
	if (!(weather && hourly && temps && rain_probs && weather_times)) {
	    fprintf(stderr, "\x1b[38;5;139m\x1b[1mERR:\x1b[0m parse weather, json <%s>\n", weather_buf);
	    fflush(stderr);
	    return NULL;
	}
   
	int time_offset = 0;
   
	while ( parse_time_offset(cJSON_GetArrayItem(weather_times, time_offset+1)->valuestring) < ctime ) { time_offset++; }
	uint32_t event_idx = 0;
	while (event_idx < weather_data->num_weather_events) {
   
	    weather_data->events[event_idx]->time = parse_time_offset(cJSON_GetArrayItem(weather_times, time_offset)->valuestring);
	    weather_data->events[event_idx]->temp_c = cJSON_GetArrayItem(temps, time_offset)->valuedouble;
	    weather_data->events[event_idx]->rain_prob = cJSON_GetArrayItem(rain_probs, time_offset)->valuedouble;
	    weather_data->events[event_idx]->wmo_code = cJSON_GetArrayItem(wmo_codes, time_offset)->valueint;
   
	    printf("\x1b[38;5;139m\x1b[1mINFO:\x1b[0m weather event %d: time=%ld, temp=%lf*C, p_precip=%lf, wmo=%d\n", event_idx, weather_data->events[event_idx]->time, weather_data->events[event_idx]->temp_c, weather_data->events[event_idx]->rain_prob / 100.0, weather_data->events[event_idx]->wmo_code);
	    fflush(stdout);
	    time_offset++;
	    event_idx++;
	}
   
	cJSON_free(weather);
	free(weather_buf);
	weather_data->last_update = ctime;
	return NULL;
}

gboolean update_weather(gpointer* data) {
	weather_t* weather_data = (weather_t*) data;

	time_t ctime;
	if ( weather_data->last_update + weather_data->update_freq > (ctime = time(NULL)) ) {
	    return TRUE;
	}

	weather_data->last_update = 2086733650;
	printf("\x1b[38;5;139m\x1b[1mINFO:\x1b[0m beginning weather update\n"); fflush(stdout);
	
	pthread_t thread_id;
	pthread_create(&thread_id, NULL, update_weather_async, data);

	printf("\x1b[38;5;139m\x1b[1mINFO:\x1b[0m weather update complete\n"); fflush(stdout);

	return TRUE;
}
