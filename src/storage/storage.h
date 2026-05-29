#pragma once
#define STORAGE_PATH "./config.dat"

#define ARRAY_KEY_COUNT 7

#define ARRAY_KEY_BRIGHTNESS 0
#define ARRAY_STRIP_GRAPH_H 1
#define ARRAY_STRIP_GRAPH_S 2
#define ARRAY_STRIP_GRAPH_V 3
#define ARRAY_LAMP_GRAPH_H 4
#define ARRAY_LAMP_GRAPH_S 5
#define ARRAY_LAMP_GRAPH_V 6
/* etc... */

void storage_init();
void write_void_ptr(int key, void *objs, int n, int stride);
void *read_void_ptr(int key, int n, int stride);
