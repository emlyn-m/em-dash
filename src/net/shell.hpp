#pragma once

#include <functional>
#include <string>

namespace ui {

struct Shell_State {
  std::string ip;
  int port;
  int timeout;

  bool sh_active;
  int sh_ecode;
  bool lg_active;
  int lg_ecode;
};

void shell_start(std::function<void()> on_update);
const Shell_State &shell_state();
void shell_tryconn();  // main thread only
void shell_dumplogs(); // main thread only

} // namespace ui
