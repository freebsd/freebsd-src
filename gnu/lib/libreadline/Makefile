#       $Id: Makefile,v 1.31 1999/05/28 04:25:45 ache Exp $

SUBDIR = history readline

SRCDIR= ${.CURDIR}/../../../contrib/libreadline

INSTALLED_HEADERS= readline.h chardefs.h keymaps.h history.h tilde.h \
		   rlstdc.h rlconf.h

beforeinstall:
.for i in ${INSTALLED_HEADERS}
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${SRCDIR}/$i \
		${DESTDIR}/usr/include/readline
.endfor

.include <bsd.subdir.mk>
