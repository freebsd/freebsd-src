# $FreeBSD$

NOMAN=YES
CLEANFILES+= ${LOCALES:S/$/.out/g}

LOCALES=	af_ZA.ISO8859-1 \
		am_ET.UTF-8 \
		be_BY.CP1251 \
		bg_BG.CP1251 \
		ca_ES.ISO8859-1 \
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
		hy_AM.ARMSCII-8 \
		is_IS.ISO8859-1 \
		it_IT.ISO8859-1 \
		ja_JP.eucJP \
		kk_KZ.PT154 \
		ko_KR.eucKR \
		lt_LT.ISO8859-13 \
		nl_NL.ISO8859-1 \
		no_NO.ISO8859-1 \
		pl_PL.ISO8859-2 \
		pt_BR.ISO8859-1 \
		pt_PT.ISO8859-1 \
		ro_RO.ISO8859-2 \
		ru_RU.KOI8-R \
		sk_SK.ISO8859-2 \
		sl_SI.ISO8859-2 \
		sr_YU.ISO8859-2 \
		sr_YU.ISO8859-5 \
		sv_SE.ISO8859-1 \
		tr_TR.ISO8859-9 \
		uk_UA.KOI8-U \
		zh_CN.eucCN

LOCALEDIR=	${DESTDIR}/usr/share/locale

ASCIILINKS=	en_US
LATIN15LINKS=   af_ZA ca_ES da_DK de_DE en_US es_ES fi_FI fr_BE fr_FR is_IS \
		it_CH it_IT nl_NL no_NO pt_PT sv_SE

UTF8_LATIN1LINKS=	af_ZA ca_ES da_DK de_DE en_US es_ES fi_FI fr_BE fr_FR \
			is_IS it_IT nl_NL no_NO pt_BR pt_PT sv_SE
UTF8_LATIN2LINKS=	cs_CZ hr_HR hu_HU pl_PL ro_RO sk_SK sl_SI
UTF8_LATIN5LINKS=	sr_YU uk_UA
UTF8_LATIN7LINKS=	el_GR
UTF8_LATIN9LINKS=	tr_TR
UTF8_LATIN13LINKS=	lt_LT
UTF8_LATIN15LINKS=	et_EE
UTF8_ARMSCII8LINKS=	hy_AM

US_LINKS=	en_CA en_GB
GB_LINKS=	en_AU en_NZ
FR_LINKS=	fr_CA fr_CH
DE_LINKS=	de_AT de_CH
IT_LINKS=	it_CH
BE_LINKS=	nl_BE
RU_LINKS=       ru_RU.CP1251 ru_RU.CP866 ru_RU.ISO8859-5
UA_LINKS=       uk_UA.ISO8859-5
BY_LINKS=	be_BY.CP1131 be_BY.ISO8859-5
CN_LINKS=       zh_CN.GB18030 zh_CN.GB2312 zh_CN.GBK zh_HK.Big5HKSCS zh_TW.Big5

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
.for link in ${RU_LINKS}
	ln -sf ../ru_RU.KOI8-R/LC_NUMERIC \
		${LOCALEDIR}/${link}/LC_NUMERIC
.endfor
.for link in ${BY_LINKS}
	ln -sf ../be_BY.CP1251/LC_NUMERIC \
		${LOCALEDIR}/${link}/LC_NUMERIC
.endfor
.for link in ${UA_LINKS}
	ln -sf ../uk_UA.KOI8-U/LC_NUMERIC \
		${LOCALEDIR}/${link}/LC_NUMERIC
.endfor
.for link in ${CN_LINKS}
	ln -sf ../zh_CN.eucCN/LC_NUMERIC \
		${LOCALEDIR}/${link}/LC_NUMERIC
.endfor
.for link in ${ASCIILINKS}
	ln -sf ../${link}.ISO8859-1/LC_NUMERIC \
		${LOCALEDIR}/${link}.US-ASCII/LC_NUMERIC
.endfor
	ln -sf ../ja_JP.eucJP/LC_NUMERIC \
		${LOCALEDIR}/ja_JP.SJIS/LC_NUMERIC
	ln -sf ../ko_KR.eucKR/LC_NUMERIC \
		${LOCALEDIR}/ko_KR.CP949/LC_NUMERIC
.for link in ${UTF8_LATIN1LINKS}
	ln -sf ../${link}.ISO8859-1/LC_NUMERIC \
	       ${LOCALEDIR}/${link}.UTF-8/LC_NUMERIC
.endfor
.for link in ${UTF8_LATIN2LINKS}
	ln -sf ../${link}.ISO8859-2/LC_NUMERIC \
	       ${LOCALEDIR}/${link}.UTF-8/LC_NUMERIC
.endfor
.for link in ${UTF8_LATIN5LINKS}
	ln -sf ../${link}.ISO8859-5/LC_NUMERIC \
	       ${LOCALEDIR}/${link}.UTF-8/LC_NUMERIC
.endfor
.for link in ${UTF8_LATIN7LINKS}
	ln -sf ../${link}.ISO8859-7/LC_NUMERIC \
	       ${LOCALEDIR}/${link}.UTF-8/LC_NUMERIC
.endfor
.for link in ${UTF8_LATIN9LINKS}
	ln -sf ../${link}.ISO8859-9/LC_NUMERIC \
	       ${LOCALEDIR}/${link}.UTF-8/LC_NUMERIC
.endfor
.for link in ${UTF8_LATIN13LINKS}
	ln -sf ../${link}.ISO8859-13/LC_NUMERIC \
	       ${LOCALEDIR}/${link}.UTF-8/LC_NUMERIC
.endfor
.for link in ${UTF8_LATIN15LINKS}
	ln -sf ../${link}.ISO8859-15/LC_NUMERIC \
	       ${LOCALEDIR}/${link}.UTF-8/LC_NUMERIC
.endfor
.for link in ${UTF8_ARMSCII8LINKS}
	ln -sf ../${link}.ARMSCII-8/LC_NUMERIC \
	       ${LOCALEDIR}/${link}.UTF-8/LC_NUMERIC
.endfor
.for link in ${US_LINKS}
	ln -sf ../en_US.UTF-8/LC_NUMERIC \
		${LOCALEDIR}/${link}.UTF-8/LC_NUMERIC
.endfor
.for link in ${GB_LINKS}
	ln -sf ../en_GB.UTF-8/LC_NUMERIC \
		${LOCALEDIR}/${link}.UTF-8/LC_NUMERIC
.endfor
	ln -sf ../en_GB.UTF-8/LC_NUMERIC \
		${LOCALEDIR}/en_IE.UTF-8/LC_NUMERIC
.for link in ${FR_LINKS}
	ln -sf ../fr_FR.UTF-8/LC_NUMERIC \
		${LOCALEDIR}/${link}.UTF-8/LC_NUMERIC
.endfor
.for link in ${DE_LINKS}
	ln -sf ../de_DE.UTF-8/LC_NUMERIC \
		${LOCALEDIR}/${link}.UTF-8/LC_NUMERIC
.endfor
.for link in ${IT_LINKS}
	ln -sf ../it_IT.UTF-8/LC_NUMERIC \
		${LOCALEDIR}/${link}.UTF-8/LC_NUMERIC
.endfor
.for link in ${BE_LINKS}
	ln -sf ../fr_BE.UTF-8/LC_NUMERIC \
		${LOCALEDIR}/${link}.UTF-8/LC_NUMERIC
.endfor
	ln -sf ../be_BY.CP1251/LC_NUMERIC \
	       ${LOCALEDIR}/be_BY.UTF-8/LC_NUMERIC
	ln -sf ../bg_BG.CP1251/LC_NUMERIC \
	       ${LOCALEDIR}/bg_BG.UTF-8/LC_NUMERIC
	ln -sf ../en_US.US-ASCII/LC_NUMERIC \
	       ${LOCALEDIR}/he_IL.UTF-8/LC_NUMERIC
	ln -sf ../ja_JP.eucJP/LC_NUMERIC \
	       ${LOCALEDIR}/ja_JP.UTF-8/LC_NUMERIC
	ln -sf ../kk_KZ.PT154/LC_NUMERIC \
	       ${LOCALEDIR}/kk_KZ.UTF-8/LC_NUMERIC
	ln -sf ../ko_KR.eucKR/LC_NUMERIC \
	       ${LOCALEDIR}/ko_KR.UTF-8/LC_NUMERIC
	ln -sf ../ru_RU.CP866/LC_NUMERIC \
	       ${LOCALEDIR}/ru_RU.UTF-8/LC_NUMERIC
	ln -sf ../zh_CN.eucCN/LC_NUMERIC \
	       ${LOCALEDIR}/zh_CN.UTF-8/LC_NUMERIC
	ln -sf ../zh_TW.Big5/LC_NUMERIC \
	       ${LOCALEDIR}/zh_HK.UTF-8/LC_NUMERIC
	ln -sf ../zh_TW.Big5/LC_NUMERIC \
	       ${LOCALEDIR}/zh_TW.UTF-8/LC_NUMERIC
	ln -sf ../lt_LT.ISO8859-13/LC_NUMERIC \
		${LOCALEDIR}/lt_LT.ISO8859-4/LC_NUMERIC

.include <bsd.prog.mk>
