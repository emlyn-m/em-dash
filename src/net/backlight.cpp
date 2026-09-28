#include "net/backlight.hpp"

#include "config.hpp"
#include "dispatch.hpp"
#include "log.hpp"

#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>

namespace ui {

namespace {

constexpr int STEPS = 1000;

std::mutex g_mu;
std::condition_variable g_cv;
int g_pending = -1;

bool read_int(const char *path, int &out) {
  if (!path)
    return false;
  FILE *fp = fopen(path, "r");
  if (!fp)
    return false;
  bool ok = fscanf(fp, "%d", &out) == 1;
  fclose(fp);
  return ok;
}

bool write_int(const char *path, int v) {
  if (!path)
    return false;
  FILE *fp = fopen(path, "w");
  if (!fp)
    return false;
  bool ok = fprintf(fp, "%d", v) > 0;
  return fclose(fp) == 0 && ok;
}

void backlight_worker(std::function<void(float)> on_read,
                      std::function<void(float)> on_change) {
  const char *path = get_attr_str("BACKLIGHT_PATH");
  int max = 0, cur = 0;
  if (!read_int(get_attr_str("BACKLIGHT_MAX_PATH"), max) || max <= 0)
    max = 0;

  // initial read
  if (max && read_int(path, cur)) {
    float level = (float)cur / max;
    post_to_main([on_read, level] { on_read(level); });
    post_to_main([on_change, level] { on_change(level); });
  } else {
    LOG(PRI_WRN, "backlight: can't read %s\n", path ? path : "(unset)");
  }

  for (;;) {
    int target;
    {
      std::unique_lock<std::mutex> lk(g_mu);
      g_cv.wait(lk, [] { return g_pending >= 0; });
      target = g_pending;
      g_pending = -1;
    }
    if (!max || !write_int(path, target * max / STEPS)) {
      LOG(PRI_ERR, "backlight: write to %s failed\n", path ? path : "(unset)");
    } else {
      float level = (float)target / STEPS;
      post_to_main([on_change, level] { on_change(level); });
    }
  }
}

} // namespace

void backlight_start(std::function<void(float)> on_read,
                     std::function<void(float)> on_change) {
  std::thread(backlight_worker, std::move(on_read), std::move(on_change))
      .detach();
}

void backlight_set(float level) {
  level = level < 0 ? 0 : (level > 1 ? 1 : level);
  {
    std::lock_guard<std::mutex> lk(g_mu);
    g_pending = (int)(level * STEPS + 0.5f);
  }
  g_cv.notify_one();
}

} // namespace ui
