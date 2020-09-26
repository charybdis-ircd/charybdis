oper_help_dir = help_dir / 'opers'
user_help_dir = help_dir / 'users'

# files in oper help that are common with user help
user_help = [
  'topic',
  'accept',
  'cmode',
  'admin',
  'names',
  'links',
  'away',
  'whowas',
  'version',
  'kick',
  'who',
  'invite',
  'quit',
  'join',
  'list',
  'nick',
  'oper',
  'part',
  'time',
  'credits',
  'motd',
  'userhost',
  'users',
  'whois',
  'ison',
  'lusers',
  'user',
  'help',
  'pass',
  'error',
  'challenge',
  'knock',
  'ping',
  'pong',
  'map',
  'trace',
  'chantrace',
  'extban',
  'monitor'
]

foreach uh_file : user_help
  install_data('opers' / uh_file, install_dir: user_help_dir, rename: uh_file)
endforeach

install_subdir('users', install_dir: user_help_dir)
install_subdir('opers', install_dir: oper_help_dir)

# indices
#user_help_index = custom_target('users-index.stamp',
#  input: 'users.cmds',
#  output: 'users/index',
#  command: ['sh', 'gen-help-index.sh', 'users', '@INPUT@', '@OUTPUT@'],
#  install: true,
#  install_dir: help_dir
#)

#oper_help_index = custom_target('opers-index.stamp',
#  input: 'opers.cmds',
#  output: 'opers/index',
#  command: ['sh', 'gen-help-index.sh', 'opers', '@INPUT@', '@OUTPUT@'],
#  install: true,
#  install_dir: help_dir
#)
