#	@(#)Makefile	8.1 (Berkeley) 6/7/93

PROG=	mklocale
SRCS=	yacc.c lex.c
CFLAGS+=-I.
CLEANFILES+=y.tab.h yacc.c lex.c
MAN1=	mklocale.1

LOCALES=        ja_JP.EUC ru_SU.KOI8-R
ISO88591LOCALES= lt_LN.ISO8859-1 # need more names here
LOCALEDIR=	${DESTDIR}/usr/share/locale

afterinstall:
	-if [ -d ${LOCALEDIR} ]; then true; else mkdir -p ${LOCALEDIR}; fi
	for locale in ${LOCALES}; do \
		mkdir -p ${LOCALEDIR}/$$locale || true; \
		mklocale -o ${LOCALEDIR}/$$locale/LC_CTYPE ${.CURDIR}/data/$$locale; \
		chmod ${BINMODE} ${LOCALEDIR}/$$locale; \
	done
	for locale in ${ISO88591LOCALES}; do \
		mkdir -p ${LOCALEDIR}/$$locale || true; \
		mklocale -o ${LOCALEDIR}/$$locale/LC_CTYPE ${.CURDIR}/data/iso_8859_1; \
		chmod ${BINMODE} ${LOCALEDIR}/$$locale; \
	done
	chown -R ${BINOWN}.${BINGRP} ${LOCALEDIR}

.include <bsd.prog.mk>
