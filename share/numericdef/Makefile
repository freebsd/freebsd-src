# $FreeBSD$

NOMAN=YES
CLEANFILES+= ${LOCALES:S/$/.out/g}

LOCALES=        af_ZA.ISO8859-1 \
		bg_BG.CP1251 \
		cs_CZ.ISO8859-2 \
		da_DK.ISO8859-1 \
		de_DE.ISO8859-1 \
		el_GR.ISO8859-7 \
		en_US.ISO8859-1 \
		es_ES.ISO8859-1 \
		et_EE.ISO8859-15 \
		fi_FI.ISO8859-1 \
		fr_FR.ISO8859-1 \
		hi_IN.ISCII-DEV \
		hr_HR.ISO8859-2 \
		hu_HU.ISO8859-2 \
		is_IS.ISO8859-1 \
		it_IT.ISO8859-1 \
		nl_NL.ISO8859-1 \
		no_NO.ISO8859-1 \
		pl_PL.ISO8859-2 \
		pt_PT.ISO8859-1 \
		ru_RU.CP866 \
		ru_RU.ISO8859-5 \
		ru_RU.KOI8-R \
		sk_SK.ISO8859-2 \
		sl_SI.ISO8859-2 \
		sv_SE.ISO8859-1 \
		tr_TR.ISO8859-9 \
		uk_UA.KOI8-U \
		ko_KR.eucKR \
		ja_JP.eucJP \
		zh_TW.Big5 \
		zh_CN.eucCN

LOCALEDIR=      ${DESTDIR}/usr/share/locale

ASCIILINKS =    en_US
8859_15_LINKS = af_ZA da_DK de_DE en_US es_ES fi_FI fr_FR is_IS is_IS \
		it_IT nl_NL no_NO pt_PT sv_SE

USENGLISH=      en_US
USENGLISHLINKS= en_CA en_GB
GBENGLISH=      en_GB
GBENGLISHLINKS= en_AU en_NZ
FRENCH=         fr_FR
FRENCHLINKS=    fr_CA fr_CH
GERMAN=         de_DE
GERMANLINKS=    de_AT

.SUFFIXES: .src .out

.src.out:
	grep -v '^#' < ${.IMPSRC} > ${.TARGET}

all: ${LOCALES:S/$/.out/g}

afterinstall:
.for lang in ${LOCALES}
	${INSTALL} ${COPY} -m 644 -o ${BINOWN} -g ${BINGRP} ${lang}.out \
			   ${LOCALEDIR}/${lang}/LC_NUMERIC
.endfor
.for link in ${8859_15_LINKS}
	ln -sf ../${link}.ISO8859-1/LC_NUMERIC \
	       ${LOCALEDIR}/${link}.ISO8859-15/LC_NUMERIC
.endfor
.for link in ${USENGLISHLINKS}
	ln -sf ../${USENGLISH}.ISO8859-1/LC_NUMERIC \
		${LOCALEDIR}/${link}.ISO8859-1/LC_NUMERIC
	ln -sf ../${USENGLISH}.ISO8859-1/LC_NUMERIC \
		${LOCALEDIR}/${link}.ISO8859-15/LC_NUMERIC
	ln -sf ../${USENGLISH}.ISO8859-1/LC_NUMERIC \
		${LOCALEDIR}/${link}.US-ASCII/LC_NUMERIC
.endfor
.for link in ${GBENGLISHLINKS}
	ln -sf ../${GBENGLISH}.ISO8859-1/LC_NUMERIC \
		${LOCALEDIR}/${link}.ISO8859-1/LC_NUMERIC
	ln -sf ../${GBENGLISH}.ISO8859-1/LC_NUMERIC \
		${LOCALEDIR}/${link}.ISO8859-15/LC_NUMERIC
	ln -sf ../${GBENGLISH}.ISO8859-1/LC_NUMERIC \
		${LOCALEDIR}/${link}.US-ASCII/LC_NUMERIC
.endfor
.for link in ${FRENCHLINKS}
	ln -sf ../${FRENCH}.ISO8859-1/LC_NUMERIC \
		${LOCALEDIR}/${link}.ISO8859-1/LC_NUMERIC
	ln -sf ../${FRENCH}.ISO8859-1/LC_NUMERIC \
		${LOCALEDIR}/${link}.ISO8859-15/LC_NUMERIC
.endfor
.for link in ${GERMANLINKS}
	ln -sf ../${GERMAN}.ISO8859-1/LC_NUMERIC \
		${LOCALEDIR}/${link}.ISO8859-1/LC_NUMERIC
	ln -sf ../${GERMAN}.ISO8859-1/LC_NUMERIC \
		${LOCALEDIR}/${link}.ISO8859-15/LC_NUMERIC
.endfor
.for link in ${ASCIILINKS}
	ln -sf ../${link}.ISO8859-1/LC_NUMERIC \
		${LOCALEDIR}/${link}.US-ASCII/LC_NUMERIC
.endfor

.include <bsd.prog.mk>
