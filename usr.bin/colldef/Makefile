# $Id: Makefile,v 1.4 1995/09/28 18:23:34 bde Exp $

PROG = colldef
LFLAGS = -8 -i
YFLAGS = -d
CFLAGS += -I. -I${.CURDIR}/../../lib/libc/locale
SRCS = parse.c scan.c
LDADD = -ll
DPADD = ${LIBL}
CLEANFILES += lex.yy.c parse.c scan.c y.tab.[ch]

LOCALES=        ru_SU.KOI8-R ru_SU.CP866 lt_LN.ISO_8859-1
LOCALEDIR=      ${DESTDIR}/usr/share/locale

afterinstall:
	for l in ${LOCALES}; do \
		colldef -o ${LOCALEDIR}/$$l/LC_COLLATE ${.CURDIR}/data/$$l; \
		chown ${BINOWN}.${BINGRP} ${LOCALEDIR}/$$l/LC_COLLATE; \
		chmod 644 ${LOCALEDIR}/$$l/LC_COLLATE; \
	done

.include <bsd.prog.mk>
