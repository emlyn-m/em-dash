project('dash', 'cpp', version: 'v1.0.0', default_options: ['cpp_std=c++17'], meson_version: '>=1.1')

gtk_dep = dependency('gtk+-2.0')
pangocairo_dep = dependency('pangocairo')
thread_dep = dependency('threads')

sources = files(
    './src/main.cpp',
    './src/theme.cpp',
    './src/dispatch.cpp',
    './src/config.cpp',
    './src/log.cpp',
    './src/net/cJSON.cpp',
    './src/net/http.cpp',
    './src/net/weather.cpp',
    './src/net/jwt.cpp',
    './src/net/alerts.cpp',
    './src/net/telem.cpp',
    './src/net/calendar.cpp',
    './src/net/tuya.cpp',
    './src/net/led.cpp',
    './src/net/findmy.cpp',
    './src/widgets/background.cpp',
    './src/widgets/button.cpp',
    './src/widgets/primitives.cpp',
    './src/widgets/weather.cpp',
    './src/widgets/telem.cpp',
    './src/widgets/calendar.cpp',
    './src/widgets/alerts.cpp',
    './src/widgets/image.cpp',
    './src/widgets/clock.cpp',
    './src/widgets/slider.cpp',
    './src/screens/nav.cpp',
    './src/screens/screen_main.cpp',
    './src/screens/screen_led.cpp',
)

# Headers are included relative to src/, e.g. #include "widgets/widgets.hpp".
include_dirs = include_directories('src')

executable('dash', sources, include_directories: include_dirs, dependencies: [gtk_dep, pangocairo_dep, thread_dep], cpp_args: '-static-libstdc++', link_args: '-static-libstdc++')
