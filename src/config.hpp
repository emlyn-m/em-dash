#define ATTR_TYPE_BOOL 1
#define ATTR_TYPE_LONG 2
#define ATTR_TYPE_STR 3
typedef int attr_type;

typedef struct attr {
  attr_type type;
  const char *key;
  void *value;
  struct attr *next;
} attr_t;

void load_config_file(const char *cfgpath);
attr_type attr_type_if_exists(const char *attr);
int get_attr_bool(const char *attr);
long get_attr_long(const char *attr);
char *get_attr_str(const char *attr);
