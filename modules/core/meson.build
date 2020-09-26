core_modules = [
  'm_ban',
  'm_die',
  'm_error',
  'm_join',
  'm_kick',
  'm_kill',
  'm_message',
  'm_mode',
  'm_modules',
  'm_nick',
  'm_part',
  'm_quit',
  'm_server',
  'm_squit'
]

foreach core_module : core_modules
  shared_module(core_module,
    '@0@.c'.format(core_module),
    link_with: [librb_lib, ircd_lib],
    include_directories: [librb_inc, base_inc],
    name_prefix: '')
endforeach
