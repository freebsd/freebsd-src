#	From: @(#)Makefile	8.1 (Berkeley) 6/7/93
#	$Id: Makefile,v 1.4 1994/04/07 14:09:53 ats Exp $

PROG=	mklocale
SRCS=	yacc.c lex.c
CFLAGS+=-I.
CLEANFILES+=y.tab.h yacc.c lex.c
BINDIR=	/usr/bin
MAN1=	mklocale.1

LOCALES=        Ja_JP.EUC POSIX Russian.koi8-r
LOCALEDIR=	${DESTDIR}/usr/share/locale

afterinstall:
	-if [ -d ${LOCALEDIR} ]; then true; else mkdir -p ${LOCALEDIR}; fi
	for locale in ${LOCALES}; do \
		mkdir -p ${LOCALEDIR}/$$locale || true; \
		mklocale -o ${LOCALEDIR}/$$locale/LC_CTYPE ${.CURDIR}/$$locale; \
		chmod ${BINMODE} ${LOCALEDIR}/$$locale; \
	done
	chown -R ${BINOWN}.${BINGRP} ${LOCALEDIR}

.include <bsd.prog.mk>
