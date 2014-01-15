#
# Rules for handling include files.
#
# $Id: elftoolchain.inc.mk 2608 2012-10-03 09:16:11Z jkoshy $

.if !defined(TOP)
.error	Make variable \"TOP\" has not been defined.
.endif

.include "${TOP}/mk/elftoolchain.os.mk"

.include <bsd.own.mk>

.if ${OS_HOST} == "DragonFly" || ${OS_HOST} == "FreeBSD" || \
	${OS_HOST} == "OpenBSD"
# Simulate <bsd.inc.mk>.

NOBINMODE?=	444		# Missing in OpenBSD's rule set.

.PHONY:		incinstall
includes:	${INCS}	incinstall
.for inc in ${INCS}
install incinstall::	${DESTDIR}${INCSDIR}/${inc}
.PRECIOUS:	${DESTDIR}${INCSDIR}/${inc}
${DESTDIR}${INCSDIR}/${inc}: ${inc}
	cmp -s $> $@ > /dev/null 2>&1 || \
		${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} $> $@
.endfor
.else	

# Provide a default 'install' target.
install:	incinstall .PHONY

# Use the standard <bsd.inc.mk>.
.include <bsd.inc.mk>
.endif
