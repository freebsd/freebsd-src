# $FreeBSD$

NOMAN=YES
CLEANFILES+= ${LOCALES:S/$/.out/g}

LOCALES=	bg_BG.CP1251 \
		el_GR.ISO8859-7 \
		ja_JP.eucJP \
		ja_JP.SJIS \
		hi_IN.ISCII-DEV \
		ko_KR.eucKR \
		la_LN.US-ASCII \
		la_LN.ISO8859-1 \
		la_LN.ISO8859-2 \
		la_LN.ISO8859-4 \
		la_LN.ISO8859-15 \
		ru_RU.CP866 \
		ru_RU.ISO8859-5 \
		ru_RU.KOI8-R \
		uk_UA.KOI8-U \
		zh_CN.eucCN \
		zh_TW.Big5

LOCALEDIR=	${DESTDIR}/usr/share/locale

ASCIILINKS = en_AU en_CA en_GB en_NZ en_US

LATIN1LINKS = \
	af_ZA da_DK de_AT de_CH de_DE en_AU en_CA en_GB en_NZ en_US \
	es_ES fi_FI fr_BE fr_CA fr_CH fr_FR is_IS it_CH it_IT nl_BE \
	nl_NL no_NO pt_PT sv_SE

LATIN15LINKS = ${LATIN1LINKS} et_EE

LATIN2LINKS = cs_CZ hr_HR hu_HU pl_PL sl_SI sk_SK

LATIN4LINKS = lt_LT

.SUFFIXES: .src .out

.src.out:
	mklocale -o ${.TARGET} ${.IMPSRC}

all: ${LOCALES:S/$/.out/g}

afterinstall:
.for locale in ${LOCALES}
	${INSTALL} ${COPY} -m 644 -o ${BINOWN} -g ${BINGRP} \
		${locale}.out ${LOCALEDIR}/${locale}/LC_CTYPE
.endfor
.for link in ${LATIN1LINKS}
	ln -fs ../la_LN.ISO8859-1/LC_CTYPE \
		       ${LOCALEDIR}/${link}.ISO8859-1/LC_CTYPE
.endfor
.for link in ${LATIN15LINKS}
	ln -fs ../la_LN.ISO8859-15/LC_CTYPE \
		       ${LOCALEDIR}/${link}.ISO8859-15/LC_CTYPE
.endfor
.for link in ${LATIN2LINKS}
	ln -fs ../la_LN.ISO8859-2/LC_CTYPE \
		${LOCALEDIR}/${link}.ISO8859-2/LC_CTYPE
.endfor
.for link in ${LATIN4LINKS}
	ln -fs ../la_LN.ISO8859-4/LC_CTYPE \
		${LOCALEDIR}/${link}.ISO8859-4/LC_CTYPE
.endfor
.for link in ${ASCIILINKS}
	ln -fs ../la_LN.US-ASCII/LC_CTYPE \
		${LOCALEDIR}/${link}.US-ASCII/LC_CTYPE
.endfor

.include <bsd.prog.mk>
