#pragma once

#include <functional>

namespace ui {

enum FindMy_InitState {
  FINDMY_INITIALIZING,
  FINDMY_INIT_FAILED,
  FINDMY_INIT_SUCCESS,
};
struct FindMy_State {
  FindMy_InitState initialized;
  bool loading;
  bool playing;
};

void findmy_start(std::function<void()> on_update);
const FindMy_State &findmy_state();
void findmy_ping(); // main thread only

} // namespace ui
