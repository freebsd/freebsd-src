# $FreeBSD$

NOMAN=YES
CLEANFILES+= ${LOCALES:S/$/.out/g}

LOCALES=	af_ZA.ISO8859-1 \
		bg_BG.CP1251 \
		cs_CZ.ISO8859-2 \
		da_DK.ISO8859-1 \
		de_DE.ISO8859-1 \
		el_GR.ISO8859-7 \
		en_US.ISO8859-1 \
		es_ES.ISO8859-1 \
		et_EE.ISO8859-15 \
		fi_FI.ISO8859-1 \
		fr_BE.ISO8859-1 \
		fr_FR.ISO8859-1 \
		hi_IN.ISCII-DEV \
		hr_HR.ISO8859-2 \
		hu_HU.ISO8859-2 \
		is_IS.ISO8859-1 \
		it_IT.ISO8859-1 \
		ja_JP.eucJP \
		ko_KR.eucKR \
		nl_NL.ISO8859-1 \
		no_NO.ISO8859-1 \
		pl_PL.ISO8859-2 \
		pt_BR.ISO8859-1 \
		pt_PT.ISO8859-1 \
		ro_RO.ISO8859-2 \
		ru_RU.CP866 \
		ru_RU.ISO8859-5 \
		ru_RU.KOI8-R \
		sk_SK.ISO8859-2 \
		sl_SI.ISO8859-2 \
		sr_YU.ISO8859-2 \
		sr_YU.ISO8859-5 \
		sv_SE.ISO8859-1 \
		tr_TR.ISO8859-9 \
		uk_UA.ISO8859-5 \
		uk_UA.KOI8-U \
		zh_CN.eucCN \
		zh_TW.Big5

LOCALEDIR=	${DESTDIR}/usr/share/locale

ASCIILINKS=	en_US
LATIN15LINKS=	af_ZA da_DK de_DE en_US es_ES fi_FI fr_BE fr_FR is_IS \
		it_CH it_IT nl_NL no_NO pt_PT sv_SE

US_LINKS=	en_CA en_GB
GB_LINKS=	en_AU en_NZ
FR_LINKS=	fr_CA fr_CH
DE_LINKS=	de_AT de_CH
IT_LINKS=	it_CH

BE_LINKS=	nl_BE

.SUFFIXES: .src .out

.src.out:
	grep -v '^#' < ${.IMPSRC} > ${.TARGET}

all: ${LOCALES:S/$/.out/g}

afterinstall:
.for lang in ${LOCALES}
	${INSTALL} -m 644 -o ${BINOWN} -g ${BINGRP} ${lang}.out \
			   ${LOCALEDIR}/${lang}/LC_NUMERIC
.endfor
.for link in ${LATIN15LINKS}
	ln -sf ../${link}.ISO8859-1/LC_NUMERIC \
	       ${LOCALEDIR}/${link}.ISO8859-15/LC_NUMERIC
.endfor
.for link in ${US_LINKS}
	ln -sf ../en_US.ISO8859-1/LC_NUMERIC \
		${LOCALEDIR}/${link}.ISO8859-1/LC_NUMERIC
	ln -sf ../en_US.ISO8859-1/LC_NUMERIC \
		${LOCALEDIR}/${link}.ISO8859-15/LC_NUMERIC
	ln -sf ../en_US.ISO8859-1/LC_NUMERIC \
		${LOCALEDIR}/${link}.US-ASCII/LC_NUMERIC
.endfor
.for link in ${GB_LINKS}
	ln -sf ../en_GB.ISO8859-1/LC_NUMERIC \
		${LOCALEDIR}/${link}.ISO8859-1/LC_NUMERIC
	ln -sf ../en_GB.ISO8859-1/LC_NUMERIC \
		${LOCALEDIR}/${link}.ISO8859-15/LC_NUMERIC
	ln -sf ../en_GB.ISO8859-1/LC_NUMERIC \
		${LOCALEDIR}/${link}.US-ASCII/LC_NUMERIC
.endfor
.for link in ${FR_LINKS}
	ln -sf ../fr_FR.ISO8859-1/LC_NUMERIC \
		${LOCALEDIR}/${link}.ISO8859-1/LC_NUMERIC
	ln -sf ../fr_FR.ISO8859-1/LC_NUMERIC \
		${LOCALEDIR}/${link}.ISO8859-15/LC_NUMERIC
.endfor
.for link in ${DE_LINKS}
	ln -sf ../de_DE.ISO8859-1/LC_NUMERIC \
		${LOCALEDIR}/${link}.ISO8859-1/LC_NUMERIC
	ln -sf ../de_DE.ISO8859-1/LC_NUMERIC \
		${LOCALEDIR}/${link}.ISO8859-15/LC_NUMERIC
.endfor
.for link in ${IT_LINKS}
	ln -sf ../it_IT.ISO8859-1/LC_NUMERIC \
		${LOCALEDIR}/${link}.ISO8859-1/LC_NUMERIC
	ln -sf ../it_IT.ISO8859-1/LC_NUMERIC \
		${LOCALEDIR}/${link}.ISO8859-15/LC_NUMERIC
.endfor
.for link in ${BE_LINKS}
	ln -sf ../fr_BE.ISO8859-1/LC_NUMERIC \
		${LOCALEDIR}/${link}.ISO8859-1/LC_NUMERIC
	ln -sf ../fr_BE.ISO8859-1/LC_NUMERIC \
		${LOCALEDIR}/${link}.ISO8859-15/LC_NUMERIC
.endfor
.for link in ${ASCIILINKS}
	ln -sf ../${link}.ISO8859-1/LC_NUMERIC \
		${LOCALEDIR}/${link}.US-ASCII/LC_NUMERIC
.endfor
	ln -sf ../ja_JP.eucJP/LC_NUMERIC \
		${LOCALEDIR}/ja_JP.SJIS/LC_NUMERIC

.include <bsd.prog.mk>
