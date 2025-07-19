project('brdf', 'c', 'cpp')

brdf_c_args = []
brdf_cpp_args = []
brdf_link_args = []
brdf_deps = []
brdf_subsystem = 'console'

if meson.get_compiler('c').get_id() == 'gcc'
  if get_option('buildtype') == 'release'
    brdf_c_args += ['-Wl,-ffunction-sections']
    brdf_cpp_args += ['-Wl,-ffunction-sections']
    brdf_link_args += ['-s', '-Wl,--gc-sections']
  endif
  if host_machine.system() == 'windows'
    brdf_link_args += ['-static-libgcc', '-static-libstdc++', '-l:libwinpthread.a']
  endif
endif

if host_machine.system() == 'windows'
  brdf_deps += dependency('glfw3', static: true)
  brdf_deps += dependency('glm', static: true)
  if get_option('buildtype') == 'release'
    brdf_subsystem = 'windows'
  endif
else
  brdf_deps += dependency('glfw3')
  brdf_deps += dependency('glm')
endif

executable('brdf',
  'src/main.cpp',
  'lib/glad.c',
  'lib/impl.cpp',
  c_args: brdf_c_args,
  cpp_args: brdf_cpp_args,
  link_args: brdf_link_args,
  dependencies: brdf_deps,
  win_subsystem: brdf_subsystem,
)
