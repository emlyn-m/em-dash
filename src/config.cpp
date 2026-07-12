#include "config.hpp"

#include "log.hpp"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>

namespace ui {

namespace {

struct Attr {
  AttrType type;
  std::string value;  // raw text; typed getters parse on demand
};

std::unordered_map<std::string, Attr> g_config;

std::string trim(const char *s) {
  while (*s && isspace((unsigned char)*s)) s++;
  const char *end = s + strlen(s);
  while (end > s && isspace((unsigned char)end[-1])) end--;
  return std::string(s, end);
}

AttrType detect_type(const std::string &val) {
  if (val == "true" || val == "false") return ATTR_BOOL;
  size_t i = (!val.empty() && val[0] == '-') ? 1 : 0;
  if (i == val.size()) return ATTR_STR;
  for (; i < val.size(); i++)
    if (!isdigit((unsigned char)val[i])) return ATTR_STR;
  return ATTR_LONG;
}

}  // namespace

void load_config_file(const char *path) {
  FILE *fptr = fopen(path, "r");
  if (!fptr) {
    // Non-fatal: modules that need the missing keys fail gracefully.
    LOG(PRI_WRN, "config %s not found, skipping\n", path);
    return;
  }

  char line[65536];
  while (fgets(line, sizeof(line), fptr)) {
    char *eq = strchr(line, '=');
    if (!eq) continue;
    *eq = '\0';

    std::string key = trim(line);
    if (key.empty() || key[0] == '#') continue;
    std::string val = trim(eq + 1);

    g_config[key] = Attr{detect_type(val), std::move(val)};
  }

  fclose(fptr);
}

AttrType attr_type_if_exists(const char *key) {
  auto it = g_config.find(key);
  return it == g_config.end() ? ATTR_NONE : it->second.type;
}

bool get_attr_bool(const char *key) {
  auto it = g_config.find(key);
  return it != g_config.end() && it->second.value == "true";
}

long get_attr_long(const char *key) {
  auto it = g_config.find(key);
  return it == g_config.end() ? 0 : atol(it->second.value.c_str());
}

const char *get_attr_str(const char *key) {
  auto it = g_config.find(key);
  return it == g_config.end() ? nullptr : it->second.value.c_str();
}

}  // namespace ui
