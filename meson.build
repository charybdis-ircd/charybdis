project('ophion', 'c',
  version : '0.8',
  license : 'GPL-2-only',
  meson_version : '>=0.50')


cc = meson.get_compiler('c')


cdata = configuration_data()

check_functions = [
  ['HAVE_SNPRINTF', 'snprintf', 'stdio.h'],
  ['HAVE_VSNPRINTF', 'vsnprintf', 'stdio.h'],
  ['HAVE_SOCKETPAIR', 'socketpair', 'sys/socket.h'],
  ['HAVE_GETTIMEOFDAY', 'gettimeofday', 'sys/time.h'],
  ['HAVE_WRITEV', 'writev', 'sys/uio.h'],
  ['HAVE_SENDMSG', 'sendmsg', 'sys/socket.h'],
  ['HAVE_GMTIME_R', 'gmtime_r', 'time.h'],
  ['HAVE_STRTOK_R', 'strtok_r', 'string.h'],
  ['HAVE_USLEEP', 'usleep', 'unistd.h'],
  ['HAVE_POSIX_SPAWN', 'posix_spawn', 'spawn.h'],
  ['HAVE_GETEXECNAME', 'getexecname', 'stdlib.h'],
  ['HAVE_STRLCAT', 'strlcat', 'string.h'],
  ['HAVE_STRLCPY', 'strlcpy', 'string.h'],
  ['HAVE_STRNDUP', 'strndup', 'string.h'],
  ['HAVE_STRNLEN', 'strnlen', 'string.h'],
  ['HAVE_STRCASESTR', 'strcasestr', 'string.h'],
  ['HAVE_STRCASECMP', 'strcasecmp', 'string.h'],
  ['HAVE_STRNCASECMP', 'strncasecmp', 'string.h'],
  ['HAVE_FSTAT', 'fstat', 'sys/stat.h'],
  ['HAVE_SIGNALFD', 'signalfd', 'sys/signalfd.h'],
  ['HAVE_SELECT', 'select', 'sys/select.h'],
  ['HAVE_POLL', 'poll', 'poll.h'],
  ['HAVE_KEVENT', 'kevent', 'sys/event.h'],
  ['HAVE_PORT_CREATE', 'port_create', 'port.h'],
  ['HAVE_EPOLL_CTL', 'epoll_ctl', 'sys/epoll.h'],
  ['HAVE_ARC4RANDOM', 'arc4random', 'stdlib.h'],
  ['HAVE_GETRUSAGE', 'getrusage', 'sys/resource.h'],
  ['HAVE_TIMERFD_CREATE', 'timerfd_create', 'sys/timerfd.h'],
]

foreach f : check_functions
  if cc.has_function(f.get(1), prefix : '#include <' + f.get(2) + '>') and cc.has_header_symbol(f.get(2), f.get(1))
    cdata.set(f.get(0), 1)
  endif
endforeach

subdir('include')
subdir('librb')
subdir('ircd')
subdir('modules')
subdir('ssld')
subdir('bandb')
subdir('wsockd')
subdir('authd')
