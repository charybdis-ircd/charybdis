project('ophion', 'c',
  version : '0.8',
  license : 'GPL-2-only',
  meson_version : '>=0.50')


cc = meson.get_compiler('c')


cdata = configuration_data()


subdir('include')
subdir('librb')
subdir('ircd')
subdir('modules')
subdir('ssld')
subdir('bandb')
subdir('wsockd')
subdir('authd')
