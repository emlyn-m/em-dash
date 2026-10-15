#pragma once

#include <ctime>
#include <functional>
#include <string>
#include <vector>

namespace ui {

struct CalEvent {
  std::string id;
  std::string title;
  time_t start_time;
  time_t end_time;
};

struct TodoTask {
  std::string id;

  std::string title;
  time_t time;

  bool net_complete;
  bool net_needsformat;
};

struct Calendar {
  std::vector<CalEvent> events;
  time_t last_update = 0;
};

struct Todos {
  std::vector<TodoTask> tasks;
  std::vector<int> complete;
  time_t last_update = 0;
};

void calendar_start(std::function<void()> cal_update,
                    std::function<void()> todo_update);
const Calendar &calendar_state();
const Todos &todo_state();

void todo_mark(int local_id);

} // namespace ui
