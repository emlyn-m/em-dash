#include "storage.h"
#include "../log.hpp"
#include "../net/cJSON.h"
#include <cstdlib>
#include <cstring>

int storage_locked = 0;
void storage_init() {
  const static int data_buf_size = 65535;

  storage_locked = 1;
  FILE *fptr = fopen(STORAGE_PATH, "r");
  if (fptr) {
    char data_buf[data_buf_size];
    fgets(data_buf, data_buf_size, fptr);
    fclose(fptr);
    cJSON *existing = cJSON_Parse(data_buf);
    if (existing && existing->type == cJSON_Array) {
      cJSON_Delete(existing);
      storage_locked = 0;
      return;
    }
  }

  LOG(PRI_WRN, "no existing storage found\n");
  fptr = fopen(STORAGE_PATH, "w");
  if (fptr == NULL) {
    LOG(PRI_ERR, "failed to generate storage for init\n");
    storage_locked = 0;
    return;
  }
  cJSON *new_obj = cJSON_CreateArray();
  for (int i = 0; i < ARRAY_KEY_COUNT; i++) {
    cJSON_AddItemToArray(new_obj, cJSON_CreateArray());
  }
  char data_buf_w[data_buf_size];
  cJSON_PrintPreallocated(new_obj, data_buf_w, data_buf_size, 0);
  fprintf(fptr, "%s\n", data_buf_w);
  fclose(fptr);

  storage_locked = 0;
}

cJSON *read_obj() {
  while (storage_locked) {
  };
  storage_locked = 1;
  FILE *fptr = fopen(STORAGE_PATH, "r");
  if (!fptr) {
    LOG(PRI_ERR, "failed to load storage for read\n");
    storage_locked = 0;
    return NULL;
  }
  const static int data_buf_size = 65535;
  char data_buf[data_buf_size];
  fgets(data_buf, data_buf_size, fptr);
  fclose(fptr);
  storage_locked = 0;

  return cJSON_Parse(data_buf);
}

void write_obj(cJSON *obj) {
  while (storage_locked) {
  };

  storage_locked = 1;
  FILE *fptr = fopen(STORAGE_PATH, "w");
  if (!fptr) {
    LOG(PRI_ERR, "failed to load storage for write\n");
    storage_locked = 0;
    return;
  }

  const static int data_buf_size = 65535;
  char data_buf[data_buf_size];
  cJSON_PrintPreallocated(obj, data_buf, data_buf_size, 0);
  fprintf(fptr, data_buf);
  fclose(fptr);
  storage_locked = 0;
}

void *read_void_ptr(int key, int n, int stride) {
  void *out = (void *)malloc(n * stride);
  cJSON *obj = read_obj();
  if (!obj) {
    return NULL;
  }
  cJSON *val = cJSON_GetArrayItem(obj, key);
  if (!val) {
    cJSON_Delete(obj);
    LOG(PRI_ERR, "failed to find key %d in array\n", key);
    return NULL;
  }

  char v;
  for (int i = 0; i < n * stride; i++) {
    v = cJSON_GetArrayItem(val, i)->valueint;
    memcpy((char *)out + i, &v, 1);
  }

  cJSON_Delete(obj);

  return out;
}

void write_void_ptr(int key, void *objs, int n, int stride) {
  cJSON *existing = read_obj();

  char v;
  int arr_size = n * stride;
  cJSON *new_arr = cJSON_CreateArray();
  for (int i = 0; i < arr_size; i++) {
    memcpy(&v, (char *)objs + i, 1);
    cJSON_AddItemToArray(new_arr, cJSON_CreateNumber(v));
  }
  cJSON_ReplaceItemInArray(existing, key, new_arr);
  write_obj(existing);

  cJSON_Delete(existing);
}
