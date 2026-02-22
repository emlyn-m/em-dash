project('dash', 'cpp', version: 'v1.0.0', default_options: ['cpp_std=c++17'], meson_version: '>=1.1')

gtk_dep = dependency('gtk+-2.0')
thread_dep = dependency('threads')

sources = files(
    './src/main.cpp',
	'./src/net/alerts.cpp',
	'./src/net/cJSON.cpp',
	'./src/net/calendar.cpp',
	'./src/net/jwt.cpp',
	'./src/net/net.cpp',
	'./src/net/telem.cpp',
	'./src/net/weather.cpp',
	'./src/net/tuya.cpp',
	'./src/screens/screens.cpp',
	'./src/screens/screen_life.cpp',
	'./src/screens/screen_ctrl.cpp',
	'./src/screens/screen_led.cpp',
	'./src/screens/table.cpp',
	'./src/widgets/3d.cpp',
	'./src/widgets/button.cpp',
	'./src/widgets/clock.cpp',
	'./src/widgets/keyboard.cpp',
	'./src/widgets/slider.cpp',
	'./src/widgets/telem.cpp',
	'./src/widgets/alerts.cpp',
	'./src/widgets/calendar.cpp',
	'./src/widgets/image.cpp',
	'./src/widgets/label.cpp',
	'./src/widgets/tasks.cpp',
	'./src/widgets/weather.cpp'
)

include_dirs = include_directories()

executable('dash', sources, include_directories: include_dirs, dependencies: [gtk_dep,thread_dep], cpp_args: '-static-libstdc++', link_args: '-static-libstdc++')
