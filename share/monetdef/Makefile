# $FreeBSD$

NOMAN=YES
CLEANFILES+= ${LOCALES:S/$/.out/g}

LOCALES=        af_ZA.ISO8859-1 \
		am_ET.UTF-8 \
		be_BY.CP1131 \
		be_BY.CP1251 \
		be_BY.ISO8859-5 \
		be_BY.UTF-8 \
		bg_BG.CP1251 \
		bg_BG.UTF-8 \
		ca_ES.ISO8859-1 \
		cs_CZ.ISO8859-2 \
		cs_CZ.UTF-8 \
		da_DK.ISO8859-1 \
		de_AT.ISO8859-1 \
		de_CH.ISO8859-1 \
		de_DE.ISO8859-1 \
		el_GR.ISO8859-7 \
		en_AU.ISO8859-1 \
		en_CA.ISO8859-1 \
		en_GB.ISO8859-1 \
		en_GB.UTF-8 \
		en_IE.UTF-8 \
		en_NZ.ISO8859-1 \
		en_US.ISO8859-1 \
		es_ES.ISO8859-1 \
		et_EE.ISO8859-15 \
		fi_FI.ISO8859-1 \
		fr_BE.ISO8859-1 \
		fr_CA.ISO8859-1 \
		fr_FR.ISO8859-1 \
		he_IL.UTF-8 \
		hi_IN.ISCII-DEV \
		hr_HR.ISO8859-2 \
		hu_HU.ISO8859-2 \
		hy_AM.ARMSCII-8 \
		hy_AM.UTF-8 \
		is_IS.ISO8859-1 \
		it_IT.ISO8859-1 \
		ja_JP.eucJP \
		ja_JP.UTF-8 \
		kk_KZ.PT154 \
		kk_KZ.UTF-8 \
		ko_KR.eucKR \
		ko_KR.UTF-8 \
		lt_LT.ISO8859-13 \
		nl_BE.ISO8859-1 \
		nl_NL.ISO8859-1 \
		no_NO.ISO8859-1 \
		pl_PL.ISO8859-2 \
		pl_PL.UTF-8 \
		pt_BR.ISO8859-1 \
		pt_PT.ISO8859-1 \
		ro_RO.ISO8859-2 \
		ru_RU.CP1251 \
		ru_RU.CP866 \
		ru_RU.ISO8859-5 \
		ru_RU.KOI8-R \
		ru_RU.UTF-8 \
		sk_SK.ISO8859-2 \
		sl_SI.ISO8859-2 \
		sr_YU.ISO8859-2 \
		sr_YU.ISO8859-5 \
		sr_YU.UTF-8 \
		sv_SE.ISO8859-1 \
		tr_TR.ISO8859-9 \
		uk_UA.ISO8859-5 \
		uk_UA.KOI8-U \
		uk_UA.UTF-8 \
		zh_CN.eucCN \
		zh_CN.GB18030 \
		zh_CN.GB2312 \
		zh_CN.UTF-8 \
		zh_HK.UTF-8 \
		zh_TW.Big5

LOCALEDIR=      ${DESTDIR}/usr/share/locale

ASCIILINKS=	en_AU en_CA en_GB en_NZ en_US
LATIN15LINKS=   af_ZA ca_ES da_DK de_AT de_CH de_DE en_AU en_CA en_GB en_NZ \
		en_US es_ES fi_FI fr_BE fr_CA fr_CH fr_FR is_IS is_IS it_IT \
		it_CH nl_BE nl_NL no_NO pt_PT sv_SE

UTF8_LATIN1LINKS=	af_ZA ca_ES da_DK de_AT de_CH de_DE en_AU en_CA en_NZ \
			en_US es_ES fi_FI fr_BE fr_CA fr_FR is_IS it_IT nl_BE \
			nl_NL no_NO pt_BR pt_PT sv_SE
UTF8_LATIN2LINKS=	hr_HR hu_HU ro_RO sk_SK sl_SI sr_YU
UTF8_LATIN7LINKS=	el_GR
UTF8_LATIN9LINKS=	tr_TR
UTF8_LATIN13LINKS=	lt_LT
UTF8_LATIN15LINKS=	et_EE

CH_SRC=		de_CH
CH_LINKS=	fr_CH it_CH

.SUFFIXES: .src .out

.src.out:
	grep -v '^#' < ${.IMPSRC} > ${.TARGET}

all: ${LOCALES:S/$/.out/g}

afterinstall:
.for lang in ${LOCALES}
	${INSTALL} -m 644 -o ${BINOWN} -g ${BINGRP} ${lang}.out \
		${LOCALEDIR}/${lang}/LC_MONETARY
.endfor
.for link in ${LATIN15LINKS}
	ln -sf ../${link}.ISO8859-1/LC_MONETARY \
	       ${LOCALEDIR}/${link}.ISO8859-15/LC_MONETARY
.endfor
.for link in ${ASCIILINKS}
	ln -sf ../${link}.ISO8859-1/LC_MONETARY \
		${LOCALEDIR}/${link}.US-ASCII/LC_MONETARY
.endfor
.for link in ${CH_LINKS}
	ln -sf ../${CH_SRC}.ISO8859-1/LC_MONETARY \
		${LOCALEDIR}/${link}.ISO8859-1/LC_MONETARY
.endfor
	ln -sf ../ja_JP.eucJP/LC_MONETARY \
		${LOCALEDIR}/ja_JP.SJIS/LC_MONETARY
	ln -sf ../ko_KR.eucKR/LC_MONETARY \
		${LOCALEDIR}/ko_KR.CP949/LC_MONETARY
	ln -sf ../zh_CN.GB2312/LC_MONETARY \
		${LOCALEDIR}/zh_CN.GBK/LC_MONETARY
.for link in ${UTF8_LATIN1LINKS}
	ln -sf ../${link}.ISO8859-1/LC_MONETARY \
		${LOCALEDIR}/${link}.UTF-8/LC_MONETARY
.endfor
.for link in ${UTF8_LATIN2LINKS}
	ln -sf ../${link}.ISO8859-2/LC_MONETARY \
		${LOCALEDIR}/${link}.UTF-8/LC_MONETARY
.endfor
.for link in ${UTF8_LATIN7LINKS}
	ln -sf ../${link}.ISO8859-7/LC_MONETARY \
		${LOCALEDIR}/${link}.UTF-8/LC_MONETARY
.endfor
.for link in ${UTF8_LATIN9LINKS}
	ln -sf ../${link}.ISO8859-9/LC_MONETARY \
		${LOCALEDIR}/${link}.UTF-8/LC_MONETARY
.endfor
.for link in ${UTF8_LATIN13LINKS}
	ln -sf ../${link}.ISO8859-13/LC_MONETARY \
		${LOCALEDIR}/${link}.UTF-8/LC_MONETARY
.endfor
.for link in ${UTF8_LATIN15LINKS}
	ln -sf ../${link}.ISO8859-15/LC_MONETARY \
		${LOCALEDIR}/${link}.UTF-8/LC_MONETARY
.endfor
.for link in ${CH_LINKS}
	ln -sf ../${CH_SRC}.UTF-8/LC_MONETARY \
		${LOCALEDIR}/${link}.UTF-8/LC_MONETARY
.endfor
	ln -sf ../zh_TW.Big5/LC_MONETARY \
		${LOCALEDIR}/zh_TW.UTF-8/LC_MONETARY
	ln -sf ../zh_HK.UTF-8/LC_MONETARY \
		${LOCALEDIR}/zh_HK.Big5HKSCS/LC_MONETARY
	ln -sf ../lt_LT.ISO8859-13/LC_MONETARY \
		${LOCALEDIR}/lt_LT.ISO8859-4/LC_MONETARY

.include <bsd.prog.mk>
