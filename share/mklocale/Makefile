# $FreeBSD$

NOMAN=YES
CLEANFILES+= ${LOCALES:S/$/.out/g}

LOCALES=	am_ET.UTF-8 \
		bg_BG.CP1251 \
		el_GR.ISO8859-7 \
		hi_IN.ISCII-DEV \
		hy_AM.ARMSCII-8 \
		ja_JP.SJIS \
		ja_JP.eucJP \
		ko_KR.CP949 \
		ko_KR.eucKR \
		la_LN.ISO8859-1 \
		la_LN.ISO8859-15 \
		la_LN.ISO8859-2 \
		la_LN.ISO8859-4 \
		la_LN.US-ASCII \
		lt_LT.ISO8859-13 \
		ru_RU.CP866 \
		ru_RU.ISO8859-5 \
		ru_RU.KOI8-R \
		uk_UA.KOI8-U \
		zh_CN.eucCN \
		zh_CN.GB18030 \
		zh_CN.GB2312 \
		zh_CN.GBK \
		zh_TW.Big5

LOCALEDIR=	${DESTDIR}/usr/share/locale

ASCIILINKS=	en_AU en_CA en_GB en_NZ en_US
LATIN1XLINKS=   af_ZA ca_ES da_DK de_AT de_CH de_DE en_AU en_CA en_GB en_NZ \
		en_US es_ES fi_FI fr_BE fr_CA fr_CH fr_FR is_IS it_CH it_IT \
		nl_BE nl_NL no_NO pt_PT sv_SE
LATIN1LINKS=	${LATIN1XLINKS} pt_BR
LATIN15LINKS=	${LATIN1XLINKS} et_EE
LATIN2LINKS=	cs_CZ hr_HR hu_HU pl_PL sl_SI sr_YU ro_RO sk_SK
LATIN4LINKS=	lt_LT
LATIN5LINKS=	sr_YU uk_UA
CP1251LINKS=    ru_RU

.SUFFIXES: .src .out

.src.out:
	mklocale -o ${.TARGET} ${.IMPSRC}

all: ${LOCALES:S/$/.out/g}

afterinstall:
.for locale in ${LOCALES}
	${INSTALL} -m 644 -o ${BINOWN} -g ${BINGRP} \
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
.for link in ${LATIN5LINKS}
	ln -fs ../ru_RU.ISO8859-5/LC_CTYPE \
		${LOCALEDIR}/${link}.ISO8859-5/LC_CTYPE
.endfor
.for link in ${CP1251LINKS}
	ln -fs ../bg_BG.CP1251/LC_CTYPE \
		${LOCALEDIR}/${link}.CP1251/LC_CTYPE
.endfor
.for link in ${ASCIILINKS}
	ln -fs ../la_LN.US-ASCII/LC_CTYPE \
		${LOCALEDIR}/${link}.US-ASCII/LC_CTYPE
.endfor
	ln -fs ../la_LN.US-ASCII/LC_CTYPE \
		${LOCALEDIR}/tr_TR.ISO8859-9/LC_CTYPE

.include <bsd.prog.mk>
