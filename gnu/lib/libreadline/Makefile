# $FreeBSD: src/gnu/lib/libreadline/Makefile,v 1.30.2.3 1999/08/29 14:28:55 peter Exp $

SUBDIR = history readline

SRCDIR= ${.CURDIR}/../../../contrib/libreadline

INSTALLED_HEADERS= readline.h chardefs.h keymaps.h history.h tilde.h \
		   rlstdc.h rlconf.h

beforeinstall:
.for i in ${INSTALLED_HEADERS}
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${SRCDIR}/$i \
		${DESTDIR}/usr/include/readline
.endfor

.include <bsd.prog.mk>
