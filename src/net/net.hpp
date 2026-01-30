#include "../widgets/widgets.hpp"
#include "./cJSON.h"
#include "glib.h"

int http_get(char* hostname, char* path, int port, char** out, time_t* pingp);

gboolean update_events(gpointer*);
int generate_gcal_jwt(char* service_email, char* privkey, int out_size, char* out);
time_t parse_gcal_datetime(cJSON* obj);

gboolean update_weather(gpointer* data);
