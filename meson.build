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

buildtype = get_option('buildtype')

# ======================
# Compiler flags
# ======================
c_args = [
  '-Wall',
  '-Wextra',
]

link_args = []

if buildtype == 'debug'
  # Debug build: maximum debuggability
  c_args += [
    '-O0',          # no optimizations
    '-g3',          # maximum debug info
    '-ggdb3',       # gdb-friendly debug info
    '-fno-omit-frame-pointer',
    '-fno-inline',
    '-fno-optimize-sibling-calls',
  ]

  link_args += [
    '-g3',
  ]

else
  # Release build: aggressive optimization
  c_args += [
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

  link_args += [
    '-Wl,--gc-sections',
    '-Wl,--strip-all',
    '-flto',
  ]
endif

# ======================
# Dependencies
# ======================
sdl2 = dependency('sdl2', required: true)
math = cc.find_library('m', required: true)
threads = dependency('threads')

deps = [
  sdl2,
  math,
  threads,
]

# ======================
# Sources
# ======================
sources = files(
  'main.c',
  'voronoi.c',
  'glyph_cache.c',
  'frame_generator.c',
)

# ======================
# Assets
# ======================
configure_file(
  input : 'font.otf',
  output : 'font.otf',
  copy : true
)

# ======================
# Target
# ======================
executable(
  'lineboil',
  sources,
  c_args : c_args,
  link_args : link_args,
  dependencies : deps,
  include_directories : include_directories('.'),
  install : false
)
