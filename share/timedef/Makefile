# $FreeBSD$

NOMAN=YES
CLEANFILES+= ${LOCALES:S/$/.out/g}

LOCALES=	am_ET.UTF-8 \
		be_BY.CP1131 \
		be_BY.CP1251 \
		be_BY.ISO8859-5 \
		be_BY.UTF-8 \
		bg_BG.CP1251 \
		bg_BG.UTF-8 \
		ca_ES.ISO8859-1 \
		ca_ES.UTF-8 \
		cs_CZ.ISO8859-2 \
		cs_CZ.UTF-8 \
		da_DK.ISO8859-1 \
		da_DK.UTF-8 \
		de_AT.ISO8859-1 \
		de_AT.UTF-8 \
		de_DE.ISO8859-1 \
		de_DE.UTF-8 \
		el_GR.ISO8859-7 \
		el_GR.UTF-8 \
		en_GB.ISO8859-1 \
		en_US.ISO8859-1 \
		es_ES.ISO8859-1 \
		es_ES.UTF-8 \
		et_EE.ISO8859-15 \
		et_EE.UTF-8 \
		fi_FI.ISO8859-1 \
		fi_FI.UTF-8 \
		fr_FR.ISO8859-1 \
		fr_FR.UTF-8 \
		he_IL.UTF-8 \
		hi_IN.ISCII-DEV \
		hr_HR.ISO8859-2 \
		hr_HR.UTF-8 \
		hu_HU.ISO8859-2 \
		hu_HU.UTF-8 \
		hy_AM.ARMSCII-8 \
		hy_AM.UTF-8 \
		is_IS.ISO8859-1 \
		is_IS.UTF-8 \
		ja_JP.eucJP \
		ja_JP.SJIS \
		ja_JP.UTF-8 \
		it_IT.ISO8859-1 \
		it_IT.UTF-8 \
		kk_KZ.PT154 \
		kk_KZ.UTF-8 \
		ko_KR.eucKR \
		ko_KR.UTF-8 \
		la_LN.ISO8859-1 \
		lt_LT.ISO8859-4 \
		lt_LT.ISO8859-13 \
		lt_LT.UTF-8 \
		nl_NL.ISO8859-1 \
		no_NO.ISO8859-1 \
		no_NO.UTF-8 \
		pl_PL.ISO8859-2 \
		pl_PL.UTF-8 \
		pt_BR.ISO8859-1 \
		pt_BR.UTF-8 \
		pt_PT.ISO8859-1 \
		pt_PT.UTF-8 \
		ro_RO.ISO8859-2 \
		ro_RO.UTF-8 \
		ru_RU.CP1251 \
		ru_RU.CP866 \
		ru_RU.ISO8859-5 \
		ru_RU.KOI8-R \
		ru_RU.UTF-8 \
		sk_SK.ISO8859-2 \
		sk_SK.UTF-8 \
		sl_SI.ISO8859-2 \
		sl_SI.UTF-8 \
		sr_YU.ISO8859-2 \
		sr_YU.ISO8859-5 \
		sr_YU.UTF-8 \
		sv_SE.ISO8859-1 \
		sv_SE.UTF-8 \
		tr_TR.ISO8859-9 \
		tr_TR.UTF-8 \
		uk_UA.ISO8859-5 \
		uk_UA.KOI8-U \
		uk_UA.UTF-8 \
		zh_CN.eucCN \
		zh_CN.GB18030 \
		zh_CN.GB2312 \
		zh_CN.UTF-8 \
		zh_TW.Big5 \
		zh_TW.UTF-8

LOCALEDIR=      ${DESTDIR}/usr/share/locale

UTF8_LATIN1LINKS=	en_GB en_US nl_NL

LATIN15LINKS=   ca_ES da_DK de_AT de_DE en_GB en_US es_ES fi_FI fr_FR is_IS \
		it_IT la_LN nl_NL no_NO pt_PT sv_SE
DE_LINKS=	de_CH
FR_LINKS=	fr_BE fr_CA fr_CH
IT_LINKS=	it_CH
LN_LINKS=	la_LN.US-ASCII la_LN.ISO8859-2 la_LN.ISO8859-4
NL_LINKS=	nl_BE
US_LINKS=
GB_LINKS=	en_AU en_CA en_NZ

.SUFFIXES: .src .out

.src.out:
	grep -v '^#' < ${.IMPSRC} > ${.TARGET}

all: ${LOCALES:S/$/.out/g}

afterinstall:
	for l in ${LOCALES}; do \
		${INSTALL} -m 644 -o ${BINOWN} -g ${BINGRP} $$l.out \
			   ${LOCALEDIR}/$$l/LC_TIME; \
	done
	for l in ${LATIN15LINKS}; do \
		ln -fs ../$$l.ISO8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.ISO8859-15/LC_TIME; \
	done
	for l in ${DE_LINKS}; do \
		ln -fs ../de_DE.ISO8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.ISO8859-1/LC_TIME; \
		ln -fs ../de_DE.ISO8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.ISO8859-15/LC_TIME; \
	done
	for l in ${IT_LINKS}; do \
		ln -fs ../it_IT.ISO8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.ISO8859-1/LC_TIME; \
		ln -fs ../it_IT.ISO8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.ISO8859-15/LC_TIME; \
	done
	for l in ${FR_LINKS}; do \
		ln -fs ../fr_FR.ISO8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.ISO8859-1/LC_TIME; \
		ln -fs ../fr_FR.ISO8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.ISO8859-15/LC_TIME; \
	done
	for l in ${LN_LINKS}; do \
		ln -fs ../la_LN.ISO8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l/LC_TIME; \
	done
	for l in ${NL_LINKS}; do \
		ln -fs ../nl_NL.ISO8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.ISO8859-1/LC_TIME; \
		ln -fs ../nl_NL.ISO8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.ISO8859-15/LC_TIME; \
	done
	for l in ${US_LINKS}; do \
		ln -fs ../en_US.ISO8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.ISO8859-1/LC_TIME; \
		ln -fs ../en_US.ISO8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.ISO8859-15/LC_TIME; \
		ln -fs ../en_US.ISO8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.US-ASCII/LC_TIME; \
	done
	for l in ${GB_LINKS}; do \
		ln -fs ../en_GB.ISO8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.ISO8859-1/LC_TIME; \
		ln -fs ../en_GB.ISO8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.ISO8859-15/LC_TIME; \
		ln -fs ../en_GB.ISO8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.US-ASCII/LC_TIME; \
	done
	ln -fs ../en_GB.ISO8859-1/LC_TIME \
		${LOCALEDIR}/en_GB.US-ASCII/LC_TIME
	ln -fs ../en_US.ISO8859-1/LC_TIME \
		${LOCALEDIR}/en_US.US-ASCII/LC_TIME
	ln -fs ../en_US.ISO8859-1/LC_TIME \
		${LOCALEDIR}/af_ZA.ISO8859-1/LC_TIME
	ln -fs ../en_US.ISO8859-15/LC_TIME \
		${LOCALEDIR}/af_ZA.ISO8859-15/LC_TIME
	ln -fs ../ko_KR.eucKR/LC_TIME \
		${LOCALEDIR}/ko_KR.CP949/LC_TIME
	ln -fs ../zh_CN.GB2312/LC_TIME \
		${LOCALEDIR}/zh_CN.GBK/LC_TIME
	for l in ${UTF8_LATIN1LINKS}; do \
		ln -fs ../$$l.ISO8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.UTF-8/LC_TIME; \
	done
	for l in ${DE_LINKS}; do \
		ln -fs ../de_DE.UTF-8/LC_TIME \
		       ${LOCALEDIR}/$$l.UTF-8/LC_TIME; \
	done
	for l in ${IT_LINKS}; do \
		ln -fs ../it_IT.UTF-8/LC_TIME \
		       ${LOCALEDIR}/$$l.UTF-8/LC_TIME; \
	done
	for l in ${FR_LINKS}; do \
		ln -fs ../fr_FR.UTF-8/LC_TIME \
		       ${LOCALEDIR}/$$l.UTF-8/LC_TIME; \
	done
	for l in ${NL_LINKS}; do \
		ln -fs ../nl_NL.UTF-8/LC_TIME \
		       ${LOCALEDIR}/$$l.UTF-8/LC_TIME; \
	done
	for l in ${US_LINKS}; do \
		ln -fs ../en_US.UTF-8/LC_TIME \
		       ${LOCALEDIR}/$$l.UTF-8/LC_TIME; \
	done
	for l in ${GB_LINKS}; do \
		ln -fs ../en_GB.UTF-8/LC_TIME \
		       ${LOCALEDIR}/$$l.UTF-8/LC_TIME; \
	done
	ln -fs ../en_GB.UTF-8/LC_TIME \
	       ${LOCALEDIR}/en_IE.UTF-8/LC_TIME
	ln -fs ../en_US.UTF-8/LC_TIME \
	       ${LOCALEDIR}/af_ZA.UTF-8/LC_TIME
	ln -fs ../zh_TW.UTF-8/LC_TIME \
		${LOCALEDIR}/zh_HK.UTF-8/LC_TIME
	ln -fs ../zh_TW.UTF-8/LC_TIME \
		${LOCALEDIR}/zh_HK.Big5HKSCS/LC_TIME

.include <bsd.prog.mk>
