#	@(#)Makefile	8.1 (Berkeley) 6/7/93

PROG=	mklocale
SRCS=	yacc.c lex.c
CFLAGS+=-I.
CLEANFILES+=y.tab.h yacc.c lex.c
MAN1=	mklocale.1
LOCALES=        ja_JP.EUC ru_SU.KOI8-R lt_LN.ISO8859-1
LOCALEDIR=	${DESTDIR}/usr/share/locale

afterinstall:
	for locale in ${LOCALES}; do \
		mklocale -o ${LOCALEDIR}/$$locale/LC_CTYPE ${.CURDIR}/data/$$locale; \
		chown ${BINOWN}.${BINGRP} ${LOCALEDIR}/$$locale; \
		chmod ${BINMODE} ${LOCALEDIR}/$$locale; \
	done

.include <bsd.prog.mk>
