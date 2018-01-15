prefix = @prefix@
exec_prefix = @execprefix@
helpdir = @helpdir@
uhelpdir = @helpdir@/users
ohelpdir = @helpdir@/opers

SYMLINKS = topic accept cmode admin names links away whowas \
	version kick who invite quit join list nick oper part \
	time credits motd userhost users whois ison lusers \
	user help pass error challenge knock ping pong \
	map trace chantrace extban monitor

all:
	@echo "Building index files"
	@${RM} users/index.tmp
	@for help in users/*; do \
		if [ -f "$$help" ]; then \
			echo $$help >> users/index.tmp; \
		fi \
	done
	@for help in ${SYMLINKS}; do \
		echo $$help >> users/index.tmp; \
	done
	@echo "Help topics available to users:" > users/index
	@echo "" >> users/index
	@cat users/index.tmp \
		| sed -e 's|^users/||' \
		| sort -u \
		| tr a-z A-Z \
		| column -c 65 -x \
		| expand \
		>> users/index
	@${RM} users/index.tmp
	@${RM} opers/index.tmp
	@for help in opers/*; do \
		if [ -f "$$help" ]; then \
			echo $$help >> opers/index.tmp; \
		fi \
	done
	@echo "Help topics available to opers:" > opers/index
	@echo "" >> opers/index
	@cat opers/index.tmp \
		| sed -e 's|^opers/||' \
		| sort -u \
		| tr a-z A-Z \
		| column -c 65 -s ' ' -x \
		| expand \
		>> opers/index
	@${RM} opers/index.tmp

install:
	-@if test -d ${DESTDIR}${helpdir}-old; then \
		${RM} -r ${DESTDIR}${helpdir}-old; \
	fi
	-@if test -d ${DESTDIR}${helpdir}; then \
		echo "Backing up old help files"; \
		${MV} ${DESTDIR}${helpdir} ${DESTDIR}${helpdir}-old; \
	fi
	@echo "Setting up help directory structure"
	@mkdir -p -m 755 ${DESTDIR}${helpdir}
	@mkdir -p -m 755 ${DESTDIR}${helpdir}/opers
	@mkdir -p -m 755 ${DESTDIR}${helpdir}/users
	@for help in opers/*; do \
		if [ -f "$$help" ]; then \
			${INSTALL_DATA} $$help ${DESTDIR}${ohelpdir}; \
		fi \
	done
	@for help in users/*; do \
		if [ -f "$$help" ]; then \
			${INSTALL_DATA} $$help ${DESTDIR}${uhelpdir}; \
		fi \
	done
	@for link in ${SYMLINKS}; do \
		${RM} ${DESTDIR}${uhelpdir}/$$link; \
		${LN} -s ${ohelpdir}/$$link ${DESTDIR}${uhelpdir}; \
	done
