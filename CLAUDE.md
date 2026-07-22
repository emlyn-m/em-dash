# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A fixed 1440×1080 landscape dashboard (executable target `dash`) for a jailbroken Kindle.
The window title carries the `L:A_N:...` hint the Kindle's KUAL/awesome launcher needs.

## Style

The visual style of the app is flat, monochrome, and generally attempting to resemble a neat,
perfect notebook. The dot-grid background and an ink shader (`library/ink.sh`) are to give it
a hand-drawn feel, as well as creating slightly more visual interest. Sharp lines, harsh contrast,
etc are all good and welcome - everything MUST be black/white or minimal (<= 5) shades of grey for actual
UI elements (not images).

## Build & run

Native (desktop dev):
```sh
ninja -C build          # build; ./build/dash to run (run from repo root — it reads ./.env)
meson setup build       # only if build/ is missing
```

Cross-compile for the Kindle (armv7, `build_kindlepw2/`) uses the crosstool-NG toolchain at
`~/x-tools/arm-kindlepw2-linux-gnueabi/`. The tree is already configured — `ninja -C build_kindlepw2`.
Binary is `-static-libstdc++`.

There is **no test suite**. Verify changes by building and running the app. 

## Deploying to the device (`library/`)

The Kindle has no SSH; deploy is netcat-based. `tx_file.sh` serves `build_kindlepw2/dash` on port
1337 and listens for the app's stdout/stderr on 1339; the device pulls it via a KUAL "book" shim.
`tx_assets.sh` ships fonts/docs/data the same way (runs `fc-cache` after fonts). `set_ip.sh` writes
the revshell/rx-file launcher books to a mounted Kindle. Edit the hardcoded IPs in these scripts for
the current network.

## Configuration

Config is loaded at startup from two files in the repo root (missing keys are non-fatal — workers
fail gracefully):
- **`.env`** — secrets, gitignored. See `.env.example` for the full key list (Google calendar,
  FindMy, weather coords, LED/tuya device creds, telemetry endpoints).
- **`.env.config`** — non-secret endpoints, update frequencies, `WEATHER_API_CMD`. Committed.

Access via `ui::get_attr_str/long/bool(key)` (`config.hpp`). Format is `KEY=value`, no quotes, no `export`.

## Architecture

Everything app-level lives in **`namespace ui`**; only vendored `net/cJSON` is global C.

**UI layer** (`src/theme.*`, `src/screens/`, `src/widgets/`)
- Screens live in a **tabless `GtkNotebook`** (`SCREEN_MAIN`, `SCREEN_LED`); navigation flips pages.
  `main.cpp` builds it; `screens/nav.hpp` has `set_stack` / `navigate` / `nav_press`.
- Every widget is a **self-drawing `GtkDrawingArea`** placed by absolute Figma coordinates on a
  `GtkFixed`. All drawing is Cairo/Pango via `pangocairo`. `theme.hpp` holds the canvas constants
  (`SCREEN_W/H`), palette, and `draw_text` helpers; call `prewarm_fonts()` once at startup.
- Buttons are wired with listeners but most are no-op (`noop_press`); **screen-switching buttons do
  work**. The calendar/telem grey boxes are intentional placeholders — leave them unless asked.

**Net/data layer** (`src/net/`) — see `net-layer-architecture` memory for depth
- **gtk-free**: modules never touch widgets. Each takes a `std::function<void()> on_update` callback
  (the screen supplies the redraw).
- **State pattern** (reference template: `weather.cpp`): a pure-data model struct + a background
  `std::thread` worker that fetches/parses off-thread, then calls
  `ui::post_to_main([...]{ swap model; on_update(); })` (`dispatch.hpp`) to apply on the GTK main
  thread. Widgets read `xxx_state()` in their expose handlers. GTK/GDK may only be touched from the
  main thread — always route worker results through `post_to_main`.
- Foundation: `config`, `log` (`LOG(PRI_x, ...)` macro, ring buffer), `net/cJSON` (vendored),
  `net/http` (`http_get`). Modules: `weather`, `alerts`, `telem`, `calendar` (+`jwt`), `findmy`,
  `tuya`/`led` (LED devices).

**LED / tuya** (`net/tuya.cpp`, `net/led.cpp`)
- `tuya` is protocol/crypto (shells out to `openssl` — it cannot be linked on the Kindle). `led` is
  one persistent comm thread per device that only connects while the device is `active` (set by the
  LED modal on open/close). Commands (`led_set_power/hsv/query`) are callable from any thread.
- **Do not send tuya HSV during slider drags** — it overloads the device. Sliders send only the final
  colour on release. Socket `recv` must loop (`read_full`) or the GCM/ECB decrypt corrupts due to
  partial reads.

## Conventions

- Headers are included relative to `src/` (e.g. `#include "widgets/widgets.hpp"`).
- New source files must be added to the `sources` list in `meson.build`.
- Keep the net layer gtk-free and route all UI updates through `post_to_main`.
- Write good C++17.
- Keep comments relatively minimal, only using them if they are actually required for understanding.
  - Docstrings and general 'then we do X' comments should be strongly avoided.

- **Git conventions**
  - Conventional commits pls!
  - Just stick to a headline in general, commit bodies should be used sparingly.
  - Please do ask before pushing any commits, **including** on non-main branches.
