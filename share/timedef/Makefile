# $FreeBSD$

NOMAN=YES
CLEANFILES+= ${LOCALES:S/$/.out/g}

LOCALES=	bg_BG.CP1251 \
		cs_CZ.ISO8859-2 \
		da_DK.ISO8859-1 \
		de_AT.ISO8859-1 \
		de_DE.ISO8859-1 \
		en_GB.ISO8859-1 \
		en_US.ISO8859-1 \
		es_ES.ISO8859-1 \
		fi_FI.ISO8859-1 \
		fr_FR.ISO8859-1 \
		hr_HR.ISO8859-2 \
		hu_HU.ISO8859-2 \
		is_IS.ISO8859-1 \
		ja_JP.eucJP     \
		ja_JP.SJIS      \
		it_IT.ISO8859-1 \
		ko_KR.eucKR     \
		la_LN.ISO8859-1 \
		lt_LT.ISO8859-4 \
		nl_NL.ISO8859-1 \
		no_NO.ISO8859-1 \
		pl_PL.ISO8859-2 \
		pt_PT.ISO8859-1 \
		ro_RO.ISO8859-2 \
		ru_RU.CP866     \
		ru_RU.ISO8859-5 \
		ru_RU.KOI8-R    \
		sk_SK.ISO8859-2 \
		sl_SI.ISO8859-2 \
		sv_SE.ISO8859-1 \
		tr_TR.ISO8859-9 \
		uk_UA.KOI8-U    \
		zh_CN.eucCN     \
		zh_TW.Big5

LOCALEDIR=      ${DESTDIR}/usr/share/locale

8859_15_LINKS = da_DK de_AT de_DE en_GB en_US es_ES fi_FI fr_FR is_IS \
		it_IT la_LN nl_NL no_NO pt_PT sv_SE
DE_LINKS =      de_CH
FR_LINKS =      fr_BE fr_CA fr_CH
IT_LINKS =      it_CH
LN_LINKS =      la_LN.US-ASCII la_LN.ISO8859-2 la_LN.ISO8859-4
NL_LINKS =      nl_BE
US_LINKS =      en_CA
GB_LINKS =      en_AU en_NZ

.SUFFIXES: .src .out

.src.out:
	grep -v '^#' < ${.IMPSRC} > ${.TARGET}

all: ${LOCALES:S/$/.out/g}

afterinstall:
	for l in ${LOCALES}; do \
		${INSTALL} ${COPY} -m 644 -o ${BINOWN} -g ${BINGRP} $$l.out \
			   ${LOCALEDIR}/$$l/LC_TIME; \
	done
	for l in ${8859_15_LINKS}; do \
		ln -fs ../$$l.ISO8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.ISO8859-15/LC_TIME; \
	done
	for l in ${DE_LINKS}; do \
		ln -fs ../de_DE.ISO8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.ISO8859-1/LC_TIME; \
	done
	for l in ${DE_LINKS}; do \
		ln -fs ../de_DE.ISO8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.ISO8859-15/LC_TIME; \
	done
	for l in ${IT_LINKS}; do \
		ln -fs ../it_IT.ISO8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.ISO8859-1/LC_TIME; \
	done
	for l in ${IT_LINKS}; do \
		ln -fs ../it_IT.ISO8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.ISO8859-15/LC_TIME; \
	done
	for l in ${FR_LINKS}; do \
		ln -fs ../fr_FR.ISO8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.ISO8859-1/LC_TIME; \
	done
	for l in ${FR_LINKS}; do \
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
	done
	for l in ${NL_LINKS}; do \
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

.include <bsd.prog.mk>
