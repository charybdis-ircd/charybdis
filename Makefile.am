SUBDIRS = librb

if BUILD_LTDL
SUBDIRS += libltdl
endif

SUBDIRS += ircd \
	ssld \
	wsockd \
	authd \
	bandb \
	tests \
	tools \
	modules \
	extensions \
	help \
	doc

BUILT_SOURCES = include/serno.h

include/serno.h:
	@if [ -d ".git" ]; then \
		revh=`git log -1 --date=format:%Y%m%d --pretty=format:%cd-%h`; \
		datecode=`git log -1 --pretty=format:%ct`; \
		if [ -n "$$revh" ]; then \
			echo '#define SERNO "'$$revh'"' > include/serno.h; \
			echo "#define DATECODE $${datecode}UL" >> include/serno.h; \
		fi \
	fi

	@if [ ! -f "include/serno.h" ]; then \
		echo '#define SERNO "unknown"' > include/serno.h; \
		echo '#define DATECODE 0UL' >> include/serno.h; \
	fi

install-data-hook:
	@test -d @logdir@ || mkdir -p @logdir@

install-exec-hook:
	@${RM} @moduledir@/*.la
	@${RM} @moduledir@/autoload/*.la
	@${RM} @moduledir@/extensions/*.la
	@${RM} @moduledir@/*.dll.a
	@${RM} @moduledir@/autoload/*.dll.a
	@${RM} @moduledir@/extensions/*.dll.a

distclean-local:
	@${RM} librb/include/librb-config.h

clean-local:
	@${RM} include/serno.h
	@${RM} ircd/ircd_lexer.c
	@${RM} ircd/ircd_parser.c
	@${RM} ircd/ircd_parser.h
	@${RM} ircd/version.c
	@${RM} ircd/version.c.last

uninstall:
	@echo "Removing @prefix@"
	@${RM} -r @prefix@
