#include "config.hpp"
#include "log.hpp"

#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

attr_t *config = NULL;

static char *trim(char *s) {
  while (isspace((unsigned char)*s))
    s++;
  char *end = s + strlen(s) - 1;
  while (end > s && isspace((unsigned char)*end))
    *end-- = '\0';
  return s;
}

static void append_attr(attr_t *node) {
  if (config == NULL) {
    config = node;
    return;
  }
  attr_t *cur = config;
  while (cur->next)
    cur = cur->next;
  cur->next = node;
}

void load_config_file(const char *cfgpath) {
  FILE *fptr = fopen(cfgpath, "r");
  if (fptr == NULL) {
    LOG(PRI_ERR, "failed to open cfg %s\n", cfgpath);
    exit(1);
  }

  char line[65536];
  while (fgets(line, sizeof(line), fptr)) {
    char *s = trim(line);

    // skip blank lines and comments
    if (*s == '\0' || *s == '#')
      continue;

    char *eq = strchr(s, '=');
    if (!eq)
      continue;

    *eq = '\0';
    char *key = trim(s);
    char *val = trim(eq + 1);

    if (*key == '\0')
      continue;

    attr_t *node = (attr_t *)malloc(sizeof(attr_t));
    node->key = strdup(key);
    node->next = NULL;

    // type detection: true/false -> bool, all digits (opt. leading -) -> int,
    // else str
    if (strcmp(val, "true") == 0 || strcmp(val, "false") == 0) {
      node->type = ATTR_TYPE_BOOL;
      node->value = (void *)((strcmp(val, "true") == 0) ? 1 : 0);
    } else {
      char *p = val;
      if (*p == '-')
        p++;
      int is_int = (*p != '\0');
      while (*p) {
        if (!isdigit((unsigned char)*p++)) {
          is_int = 0;
          break;
        }
      }

      if (is_int) {
        node->type = ATTR_TYPE_LONG;
        node->value = (void *)atol(val);
      } else {
        node->type = ATTR_TYPE_STR;
        node->value = strdup(val);
      }
    }

    append_attr(node);
  }

  fclose(fptr);
}

static attr_t *find_attr(const char *key) {
  for (attr_t *cur = config; cur; cur = cur->next)
    if (strcmp(cur->key, key) == 0)
      return cur;
  return NULL;
}

attr_type attr_type_if_exists(const char *key) {
  attr_t *a = find_attr(key);
  return a ? a->type : 0;
}

int get_attr_bool(const char *key) {
  attr_t *a = find_attr(key);
  if (!a || a->type != ATTR_TYPE_BOOL)
    return 0;
  return reinterpret_cast<long>(a->value);
}

long get_attr_long(const char *key) {
  attr_t *a = find_attr(key);
  if (!a || a->type != ATTR_TYPE_LONG)
    return 0;
  return reinterpret_cast<long>(a->value);
}

char *get_attr_str(const char *key) {
  attr_t *a = find_attr(key);
  if (!a || a->type != ATTR_TYPE_STR)
    return NULL;
  return (char *)a->value;
}
