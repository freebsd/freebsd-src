# $FreeBSD$

NOMAN=YES
CLEANFILES+= ${LOCALES:S/$/.out/g}

LOCALES=        ja_JP.EUC ru_SU.KOI8-R lt_LN.ASCII lt_LN.ISO_8859-1 ru_SU.CP866
LOCALEDIR=	${DESTDIR}/usr/share/locale

.if exists(${.OBJDIR}/..)
MKLOCALE=${.OBJDIR}/../mklocale
.else
MKLOCALE=${.CURDIR}/../mklocale
.endif

LATIN1LINKS = \
	da_DK de_AT de_CH de_DE en_AU en_CA en_GB en_US es_ES fi_FI \
	fr_BE fr_CA fr_CH fr_FR is_IS it_CH it_IT nl_BE nl_NL no_NO \
	pt_PT sv_SE

.SUFFIXES: .src .out

.src.out:
	${MKLOCALE} -o ${.TARGET} ${.IMPSRC}

all: ${LOCALES:S/$/.out/g}

afterinstall:
	for locale in ${LOCALES}; do \
		${INSTALL} ${COPY} -m 644 -o ${BINOWN} -g ${BINGRP} $$locale.out ${LOCALEDIR}/$$locale/LC_CTYPE; \
	done
	for l in ${LATIN1LINKS}; do \
		ln -fs ../lt_LN.ISO_8859-1/LC_CTYPE \
		       ${LOCALEDIR}/$$l.ISO_8859-1/LC_CTYPE; \
	done

.include <bsd.prog.mk>
