# $Id: Makefile,v 1.1.1.1 1995/02/17 17:29:49 ache Exp $

PROG = colldef
LFLAGS = -8 -i
YFLAGS = -d
CFLAGS += -I. -I${.CURDIR}/../../lib/libc/locale
SRCS = parse.y scan.l
LDADD = -ll
DPADD = ${LIBL}
CLEANFILES = y.tab.[ch] lex.yy.c

LOCALES=        ru_SU.KOI8-R ru_SU.CP866 lt_LN.ISO8859-1
LOCALEDIR=      ${DESTDIR}/usr/share/locale

afterinstall:
	for l in ${LOCALES}; do \
		colldef -o ${LOCALEDIR}/$$l/LC_COLLATE ${.CURDIR}/data/$$l; \
		chown ${BINOWN}.${BINGRP} ${LOCALEDIR}/$$l/LC_COLLATE; \
		chmod 644 ${LOCALEDIR}/$$l/LC_COLLATE; \
	done

.include <bsd.prog.mk>
