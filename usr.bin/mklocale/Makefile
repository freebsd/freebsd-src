#	@(#)Makefile	8.1 (Berkeley) 6/7/93

PROG=	mklocale
SRCS=	yacc.c lex.c
CFLAGS+=-I.
CLEANFILES+=y.tab.h yacc.c lex.c
MAN1=	mklocale.1
ISO88591=       lt_LN.ISO8859-1
ISO88591NAMES=  da_DK de_DE de_AT de_CH en_GB en_AU en_CA en_US \
		es_ES fi_FI fr_FR fr_BE fr_CA fr_CH is_IS it_IT it_CH \
		nl_NL nl_BE no_NO pt_PT sv_SE
LOCALES=        ja_JP.EUC ru_SU.KOI8-R ${ISO88591}
LOCALEDIR=	${DESTDIR}/usr/share/locale

afterinstall:
	-if [ -d ${LOCALEDIR} ]; then true; else mkdir -p ${LOCALEDIR}; fi
	for locale in ${LOCALES}; do \
		mkdir -p ${LOCALEDIR}/$$locale || true; \
		mklocale -o ${LOCALEDIR}/$$locale/LC_CTYPE ${.CURDIR}/data/$$locale; \
		chmod ${BINMODE} ${LOCALEDIR}/$$locale; \
	done
	cd ${LOCALEDIR}; for locale in ${ISO88591NAMES}; do \
		ln -s ${ISO88591} $$locale.ISO8859-1; \
	done
	chown -R ${BINOWN}.${BINGRP} ${LOCALEDIR}

.include <bsd.prog.mk>
