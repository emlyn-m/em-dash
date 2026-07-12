project('dash', 'cpp', version: 'v1.0.0', default_options: ['cpp_std=c++17'], meson_version: '>=1.1')

gtk_dep = dependency('gtk+-2.0')
thread_dep = dependency('threads')

sources = files(
    './src/main.cpp'
)

include_dirs = include_directories()

executable('dash', sources, include_directories: include_dirs, dependencies: [gtk_dep,thread_dep], cpp_args: '-static-libstdc++', link_args: '-static-libstdc++')
