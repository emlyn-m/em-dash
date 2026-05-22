#pragma once
#define STORAGE_PATH "/home/emlyn/pets/kindle/config.dat"

#define ARRAY_KEY_COUNT 1
#define ARRAY_KEY_BRIGHTNESS 0
/* etc... */

void storage_init();
void write_void_ptr(int key, void *objs, int n, int stride);
void *read_void_ptr(int key, int n, int stride);
