check_PROGRAMS = runtests \
	msgbuf_parse1 \
	msgbuf_unparse1 \
	rb_linebuf_put1 \
	substitution1
AM_CFLAGS=$(WARNFLAGS)
AM_CPPFLAGS = $(DEFAULT_INCLUDES) -I../librb/include -I..
AM_LDFLAGS = -no-install
LDADD = tap/libtap.a ../librb/src/librb.la ../ircd/libircd.la

# Override -rpath or programs will be linked to installed libraries
libdir=$(abs_top_builddir)

runtests_CPPFLAGS = -DC_TAP_SOURCE='"$(abs_top_srcdir)/tests"' \
	-DC_TAP_BUILD='"$(abs_top_builddir)/tests"'
check_LIBRARIES = tap/libtap.a
tap_libtap_a_SOURCES = tap/basic.c tap/basic.h \
	tap/float.c tap/float.h tap/macros.h

msgbuf_parse1_SOURCES = msgbuf_parse1.c
msgbuf_unparse1_SOURCES = msgbuf_unparse1.c
rb_linebuf_put1_SOURCES = rb_linebuf_put1.c
substitution1_SOURCES = substitution1.c

check-local: $(check_PROGRAMS)
	./runtests -l $(abs_top_srcdir)/tests/TESTS
