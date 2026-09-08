project('ceddec', 'cpp',
  version : '0.0.1',
  license : 'MIT',
  default_options : [
    'cpp_std=c++20',
    'warning_level=2',
    'default_library=shared',
  ],
)

public_inc = include_directories('include')

root_inc = include_directories('.')
misc_inc = include_directories('source/misc')

ceddec_sources = files(
  'source/parser.cpp',
  'source/ir_lifter.cpp',
  'source/printer.cpp',
  'source/formatter.cpp',
  'source/ast_builder.cpp',
  'source/emitters/base.cpp',
  'source/emitters/c.cpp',
)

ceddec_api = library('ceddec_api',
  ceddec_sources,
  include_directories : [public_inc, root_inc, misc_inc],
  cpp_args : ['-DCEDDEC_BUILDING'],
  gnu_symbol_visibility : 'hidden',
  version : meson.project_version(),
  install : true,
)

ceddec_dep = declare_dependency(
  include_directories : public_inc,
  link_with : ceddec_api,
)

install_subdir('include/ceddec', install_dir : get_option('includedir'))

pkg = import('pkgconfig')
pkg.generate(ceddec_api,
  name : 'ceddec_api',
  description : 'x86/x86-64 disassembly-to-C decompiler pipeline',
  version : meson.project_version(),
)

if get_option('tests')
  executable('test1',
    '.tests/test1.cpp',
    dependencies : ceddec_dep,
  )
endif

if get_option('cli')
  executable('ceddec',
    '.tests/ceddec.cpp',
    dependencies : ceddec_dep,
  )
endif
