#	@(#)Makefile	8.1 (Berkeley) 6/7/93

PROG=	mklocale
SRCS=	yacc.c lex.c
CFLAGS+=-I.
CLEANFILES+=y.tab.h yacc.c lex.c
MAN1=	mklocale.1
LOCALES=        ja_JP.EUC ru_SU.KOI8-R lt_LN.ISO_8859-1 ru_SU.CP866
LOCALEDIR=	${DESTDIR}/usr/share/locale

LATIN1LINKS = \
	da_DK de_AT de_CH de_DE en_AU en_CA en_GB en_US es_ES fi_FI \
	fr_BE fr_CA fr_CH fr_FR is_IS it_CH it_IT nl_BE nl_NL no_NO \
	pt_PT sv_SE

afterinstall:
	for locale in ${LOCALES}; do \
		mklocale -o ${LOCALEDIR}/$$locale/LC_CTYPE ${.CURDIR}/data/$$locale; \
		chown ${BINOWN}.${BINGRP} ${LOCALEDIR}/$$locale/LC_CTYPE; \
		chmod 644 ${LOCALEDIR}/$$locale/LC_CTYPE; \
	done
	for l in ${LATIN1LINKS}; do \
		ln -fs ../lt_LN.ISO_8859-1/LC_CTYPE \
		       ${LOCALEDIR}/$$l.ISO_8859-1/LC_CTYPE; \
	done

.include <bsd.prog.mk>
