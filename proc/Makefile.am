AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/librb/include $(LTDLINCL)
AM_LDFLAGS  = -module -export-dynamic -avoid-version -no-undefined -shared
AM_LDFLAGS += -export-symbols-regex _mheader -Wl,-pie,-export-dynamic
LIBS += $(top_srcdir)/ircd/libircd.la
LIBS += $(top_srcdir)/librb/src/librb.la

proc_moddir=@moduledir@/proc

p_example_la_SOURCES = \
	p_example.c \
	proc_module.c

proc_mod_LTLIBRARIES = \
	p_example.la
