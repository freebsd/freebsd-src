# $FreeBSD$

NOMAN=YES
CLEANFILES+= ${LOCALES:S/$/.out/g}

LOCALES=	cs_CZ.ISO_8859-2 \
		de_DE.DIS_8859-15 \
		de_DE.ISO_8859-1 \
		es_ES.DIS_8859-15 \
		es_ES.ISO_8859-1 \
		is_IS.DIS_8859-15 \
		is_IS.ISO_8859-1 \
		la_LN.ASCII      \
		la_LN.DIS_8859-15 \
		la_LN.ISO_8859-1 \
		la_LN.ISO_8859-2 \
		la_LN.ISO_8859-4 \
		lt_LT.ISO_8859-4 \
		ru_SU.CP866 \
		ru_SU.ISO_8859-5 \
		ru_SU.KOI8-R \
		sv_SE.DIS_8859-15 \
		sv_SE.ISO_8859-1 \
		uk_UA.KOI8-U

LOCALEDIR=      ${DESTDIR}/usr/share/locale

ASCIILINKS = \
	ko_KR.EUC ja_JP.EUC ja_JP.SJIS ja_JP.Shift_JIS zh_CN.EUC

ZHLINKS =    zh_TW.Big5

LATIN1LINKS = \
	da_DK en_AU en_CA en_GB en_US fi_FI \
	fr_BE fr_CA fr_CH fr_FR it_CH it_IT nl_BE nl_NL no_NO \
	pt_PT

LATIN2LINKS = hr_HR hu_HU pl_PL sl_SI

DELINKS = de_AT de_CH

.SUFFIXES: .src .out

.src.out:
	colldef -I ${.CURDIR} -o ${.TARGET} ${.IMPSRC}

all: ${LOCALES:S/$/.out/g}

cs_CZ.ISO_8859-2.out:	map.ISO_8859-2
de_DE.DIS_8859-15.out:  map.DIS_8859-15
de_DE.ISO_8859-1.out:   map.ISO_8859-1
es_ES.DIS_8859-15.out:  map.DIS_8859-15
es_ES.ISO_8859-1.out:   map.ISO_8859-1
is_IS.DIS_8859-15.out:  map.DIS_8859-15
is_IS.ISO_8859-1.out:   map.ISO_8859-1
la_LN.DIS_8859-15.out:  map.DIS_8859-15
la_LN.ISO_8859-1.out:   map.ISO_8859-1
la_LN.ISO_8859-4.out:   map.ISO_8859-4
lt_LT.ISO_8859-4.out:   map.ISO_8859-4
ru_SU.CP866.out:        map.CP866
ru_SU.ISO_8859-5.out:   map.ISO_8859-5
ru_SU.KOI8-R.out:       map.KOI8-R
sv_SE.DIS_8859-15.out:  map.DIS_8859-15
sv_SE.ISO_8859-1.out:   map.ISO_8859-1
uk_UA.KOI8-U.out:       map.KOI8-U

afterinstall:
.for locale in ${LOCALES}
	${INSTALL} -c -m 644 -o ${BINOWN} -g ${BINGRP} \
		${locale}.out ${LOCALEDIR}/${locale}/LC_COLLATE
.endfor
.for link in ${ASCIILINKS}
	ln -fs ../la_LN.ASCII/LC_COLLATE \
	       ${LOCALEDIR}/${link}/LC_COLLATE
.endfor
.for link in ${ZHLINKS}
	ln -fs ../is_IS.ISO_8859-1/LC_COLLATE \
	       ${LOCALEDIR}/${link}/LC_COLLATE
.endfor
.for link in ${LATIN1LINKS}
	ln -fs ../la_LN.ISO_8859-1/LC_COLLATE \
	       ${LOCALEDIR}/${link}.ISO_8859-1/LC_COLLATE
.endfor
.for link in ${LATIN1LINKS}
	ln -fs ../la_LN.DIS_8859-15/LC_COLLATE \
	       ${LOCALEDIR}/${link}.DIS_8859-15/LC_COLLATE
.endfor
.for link in ${LATIN2LINKS}
	ln -fs ../la_LN.ISO_8859-2/LC_COLLATE \
	       ${LOCALEDIR}/${link}.ISO_8859-2/LC_COLLATE
.endfor
.for link in ${DELINKS}
	ln -fs ../de_DE.ISO_8859-1/LC_COLLATE \
	       ${LOCALEDIR}/${link}.ISO_8859-1/LC_COLLATE
.endfor
.for link in ${DELINKS}
	ln -fs ../de_DE.DIS_8859-15/LC_COLLATE \
	       ${LOCALEDIR}/${link}.DIS_8859-15/LC_COLLATE
.endfor

.include <bsd.prog.mk>
