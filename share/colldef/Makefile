# $Id: Makefile,v 1.12 1997/11/07 15:40:55 ache Exp $

NOMAN=YES
CLEANFILES+= ${LOCALES:S/$/.out/g}

LOCALES=        de_DE.ISO_8859-1 \
		es_ES.ISO_8859-1 \
		is_IS.ISO_8859-1 \
		lt_LN.ASCII      \
		lt_LN.ISO_8859-1 \
		lt_LN.ISO_8859-2 \
		ru_SU.CP866 \
		ru_SU.KOI8-R

LOCALEDIR=      ${DESTDIR}/usr/share/locale

ASCIILINKS = \
	ko_KR.EUC ja_JP.EUC ja_JP.SJIS

LATIN1LINKS = \
	da_DK en_AU en_CA en_GB en_US fi_FI \
	fr_BE fr_CA fr_CH fr_FR it_CH it_IT nl_BE nl_NL no_NO \
	pt_PT sv_SE

LATIN2LINKS = hr_HR pl_PL sl_SI

DELINKS = de_AT de_CH

.SUFFIXES: .src .out

.src.out:
	colldef -I ${.CURDIR} -o ${.TARGET} ${.IMPSRC}

.if !defined(_BUILD_TOOLS)
all: ${LOCALES:S/$/.out/g}
.endif

ru_SU.KOI8-R.out:       map.KOI8-R
ru_SU.CP866.out:        map.CP866
lt_LN.ISO_8859-1.out:   map.ISO_8859-1
lt_LN.ISO_8859-2.out:   map.ISO_8859-2
es_ES.ISO_8859-1.out:   map.ISO_8859-1
de_DE.ISO_8859-1.out:   map.ISO_8859-1

.if !defined(_BUILD_TOOLS)
afterinstall:
.for locale in ${LOCALES}
	${INSTALL} -c -m 644 -o ${BINOWN} -g ${BINGRP} \
		${locale}.out ${LOCALEDIR}/${locale}/LC_COLLATE
.endfor
.for link in ${ASCIILINKS}
	ln -fs ../lt_LN.ASCII/LC_COLLATE \
	       ${LOCALEDIR}/${link}/LC_COLLATE
.endfor
.for link in ${LATIN1LINKS}
	ln -fs ../lt_LN.ISO_8859-1/LC_COLLATE \
	       ${LOCALEDIR}/${link}.ISO_8859-1/LC_COLLATE
.endfor
.for link in ${LATIN2LINKS}
	ln -fs ../lt_LN.ISO_8859-2/LC_COLLATE \
	       ${LOCALEDIR}/${link}.ISO_8859-2/LC_COLLATE
.endfor
.for link in ${DELINKS}
	ln -fs ../de_DE.ISO_8859-1/LC_COLLATE \
	       ${LOCALEDIR}/${link}.ISO_8859-1/LC_COLLATE
.endfor
.endif

.include <bsd.prog.mk>
