sqlite_dep = dependency('sqlite3', required: true)

librsdb_lib = static_library('rsdb',
  'rsdb_snprintf.c',
  'rsdb_sqlite3.c',
  include_directories: [librb_inc, base_inc])

bantool_exe = executable('bantool',
  'bantool.c',
  dependencies: [sqlite_dep],
  link_with: [librb_lib, librsdb_lib],
  install: true,
  include_directories: [librb_inc, base_inc])

bandb_exe = executable('bandb',
  'bandb.c',
  dependencies: [sqlite_dep],
  link_with: [librb_lib, librsdb_lib],
  install: true,
  include_directories: [librb_inc, base_inc])
