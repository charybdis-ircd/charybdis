SUBDIRS = src

pkgconfigdir = ${libdir}/pkgconfig
pkgconfig_DATA = librb.pc

BUILT_SOURCES = include/serno.h

# NOTE: you'll want to change the below if you are unbundling
include/serno.h:
	@if [ -f "../include/serno.h" ]; then \
		${CP} ../include/serno.h include/; \
	else \
		echo '#define SERNO "unknown"' > include/serno.h; \
	fi

clean-local:
	@${RM} include/serno.h
	@${RM} src/version.c
	@${RM} src/version.c.last
