project(
  'lineboil',
  'c',
  default_options : [
    'buildtype=release',
    'c_std=c23'
  ]
)

cc = meson.get_compiler('c')

# Hard requirement: clang only
if cc.get_id() != 'clang'
  error('This project requires clang. Configure with --native-file clang.ini')
endif

# Compiler flags
c_args = [
  '-Wall',
  '-Wextra',
  '-O3',
  '-march=native',
  '-flto',
  '-ffast-math',
  '-fno-ident',
  '-fno-asynchronous-unwind-tables',
  '-fno-stack-protector',
  '-funroll-loops',
  '-fomit-frame-pointer',
  '-ffunction-sections',
  '-fdata-sections',
  '-ffreestanding',
  '-fno-exceptions',
]

# Linker flags
link_args = [
  '-Wl,--gc-sections',
  '-Wl,--strip-all',
  '-flto',
]

# Dependencies
sdl2 = dependency('sdl2', required: true)
math = cc.find_library('m', required: true)
threads = dependency('threads')

deps = [
  sdl2,
  math,
  threads,
]

# Sources
sources = files(
  'main.c',
  'voronoi.c',
  'glyph_cache.c',
  'frame_generator.c',
)

# Target
executable(
  'lineboil',
  sources,
  c_args : c_args,
  link_args : link_args,
  dependencies : deps,
  include_directories : include_directories('.'),
  install : false
)

configure_file(
  input : 'font.otf',
  output : 'font.otf',
  copy : true
)
