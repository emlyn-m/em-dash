#pragma once

// Tiny `.env`-style config: `KEY=value` lines, `#` comments. Values are typed
// by shape — `true`/`false` -> bool, all-digits (opt. leading `-`) -> long,
// otherwise string. Loaded once at startup and never mutated, so returned
// string pointers stay valid for the program's lifetime.
namespace ui {

enum AttrType { ATTR_NONE = 0, ATTR_BOOL = 1, ATTR_LONG = 2, ATTR_STR = 3 };

void load_config_file(const char *path);

AttrType attr_type_if_exists(const char *key);
bool get_attr_bool(const char *key);
long get_attr_long(const char *key);
const char *get_attr_str(const char *key);  // nullptr if the key is absent

}  // namespace ui
