# $FreeBSD$

NOMAN=YES
CLEANFILES+= ${LOCALES:S/$/.out/g}

LOCALES=	cs_CZ.ISO_8859-2 \
		da_DK.ISO_8859-1 \
		de_AT.ISO_8859-1 \
		de_DE.ISO_8859-1 \
		en_GB.ISO_8859-1 \
		en_US.ISO_8859-1 \
		es_ES.ISO_8859-1 \
		fi_FI.ISO_8859-1 \
		fr_FR.ISO_8859-1 \
		hr_HR.ISO_8859-2 \
		hu_HU.ISO_8859-2 \
		is_IS.ISO_8859-1 \
		ja_JP.EUC	 \
		ja_JP.SJIS 	 \
		it_IT.ISO_8859-1 \
		ko_KR.EUC        \
		la_LN.ISO_8859-1 \
		lt_LT.ISO_8859-4 \
		nl_NL.ISO_8859-1 \
		no_NO.ISO_8859-1 \
		pl_PL.ISO_8859-2 \
		pt_PT.ISO_8859-1 \
		ru_SU.CP866 	 \
		ru_SU.ISO_8859-5 \
		ru_SU.KOI8-R     \
		sl_SI.ISO_8859-2 \
		sv_SE.ISO_8859-1 \
		tr_TR.ISO_8859-9 \
		uk_UA.KOI8-U     \
		zh_CN.EUC	 \
		zh_TW.Big5

LOCALEDIR=      ${DESTDIR}/usr/share/locale

8859_15_LINKS = da_DK de_AT de_DE en_GB en_US es_ES fi_FI fr_FR is_IS \
		it_IT la_LN nl_NL no_NO pt_PT sv_SE
DE_LINKS =      de_CH
FR_LINKS =      fr_BE fr_CA fr_CH
IT_LINKS =      it_CH
LN_LINKS =      la_LN.ASCII la_LN.ISO_8859-2 la_LN.ISO_8859-4
NL_LINKS =      nl_BE
US_LINKS =      en_CA
GB_LINKS =      en_AU
JP_LINKS = 	ja_JP.Shift_JIS

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
		ln -fs ../$$l.ISO_8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.DIS_8859-15/LC_TIME; \
	done
	for l in ${DE_LINKS}; do \
		ln -fs ../de_DE.ISO_8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.ISO_8859-1/LC_TIME; \
	done
	for l in ${DE_LINKS}; do \
		ln -fs ../de_DE.ISO_8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.DIS_8859-15/LC_TIME; \
	done
	for l in ${IT_LINKS}; do \
		ln -fs ../it_IT.ISO_8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.ISO_8859-1/LC_TIME; \
	done
	for l in ${IT_LINKS}; do \
		ln -fs ../it_IT.ISO_8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.DIS_8859-15/LC_TIME; \
	done
	for l in ${FR_LINKS}; do \
		ln -fs ../fr_FR.ISO_8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.ISO_8859-1/LC_TIME; \
	done
	for l in ${FR_LINKS}; do \
		ln -fs ../fr_FR.ISO_8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.DIS_8859-15/LC_TIME; \
	done
	for l in ${LN_LINKS}; do \
		ln -fs ../la_LN.ISO_8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l/LC_TIME; \
	done
	for l in ${NL_LINKS}; do \
		ln -fs ../nl_NL.ISO_8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.ISO_8859-1/LC_TIME; \
	done
	for l in ${NL_LINKS}; do \
		ln -fs ../nl_NL.ISO_8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.DIS_8859-15/LC_TIME; \
	done
	for l in ${US_LINKS}; do \
		ln -fs ../en_US.ISO_8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.ISO_8859-1/LC_TIME; \
	done
	for l in ${US_LINKS}; do \
		ln -fs ../en_US.ISO_8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.DIS_8859-15/LC_TIME; \
	done
	for l in ${GB_LINKS}; do \
		ln -fs ../en_GB.ISO_8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.ISO_8859-1/LC_TIME; \
	done
	for l in ${GB_LINKS}; do \
		ln -fs ../en_GB.ISO_8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.DIS_8859-15/LC_TIME; \
	done
	for l in ${JP_LINKS}; do \
		ln -fs ../ja_JP.SJIS/LC_TIME \
		       ${LOCALEDIR}/$$l/LC_TIME; \
	done

.include <bsd.prog.mk>
