librb_version_c = custom_target(
  'version.c',
  input: 'version.c.SH',
  output: 'version.c',
  command: ['sh', '@INPUT@', '@OUTPUT@']
)

if mbedtls_dep.found()
  tls_dep = mbedtls_dep
elif gnutls_dep.found()
  tls_dep = gnutls_dep
elif libssl_dep.found()
  tls_dep = libssl_dep
else
  tls_dep = dependency()
endif

librb_lib = shared_library('rb',
  'unix.c',
  'win32.c',
  'crypt.c',
  'balloc.c',
  'commio.c',
  'mbedtls.c',
  'openssl.c',
  'gnutls.c',
  'nossl.c',
  'event.c',
  'rb_lib.c',
  'rb_memory.c',
  'linebuf.c',
  'tools.c',
  'helper.c',
  'devpoll.c',
  'epoll.c',
  'poll.c',
  'ports.c',
  'sigio.c',
  'select.c',
  'kqueue.c',
  'rawbuf.c',
  'patricia.c',
  'dictionary.c',
  'radixtree.c',
  'arc4random.c',
  librb_version_c,
  librb_serno_h,
  dependencies: [tls_dep],
  install: true,
  include_directories: [librb_inc]
)
