# $Id: Makefile,v 1.2.2.10 1998/01/24 13:11:39 helbig Exp $

NOMAN=YES
CLEANFILES+= ${LOCALES:S/$/.out/g}

LOCALES=        da_DK.ISO_8859-1 \
		de_AT.ISO_8859-1 \
		de_DE.ISO_8859-1 \
		en_GB.ISO_8859-1 \
		en_US.ISO_8859-1 \
		es_ES.ISO_8859-1 \
		fi_FI.ISO_8859-1 \
		fr_FR.ISO_8859-1 \
		hr_HR.ISO_8859-2 \
		is_IS.ISO_8859-1 \
		ja_JP.EUC	 \
		ja_JP.SJIS 	 \
		it_IT.ISO_8859-1 \
		ko_KR.EUC        \
		lt_LN.ISO_8859-1 \
		nl_NL.ISO_8859-1 \
		no_NO.ISO_8859-1 \
		pl_PL.ISO_8859-2 \
		pt_PT.ISO_8859-1 \
		ru_SU.CP866 	 \
		ru_SU.KOI8-R     \
		sl_SI.ISO_8859-2 \
		sv_SE.ISO_8859-1

LOCALEDIR=      ${DESTDIR}/usr/share/locale

DE_LINKS =      de_CH
FR_LINKS =      fr_BE fr_CA fr_CH
IT_LINKS =      it_CH
LN_LINKS =      lt_LN.ASCII lt_LN.ISO_8859-2
NL_LINKS =      nl_BE
US_LINKS =      en_AU en_CA

.SUFFIXES: .src .out

.src.out:
	grep -v '^#' < ${.IMPSRC} > ${.TARGET}

all: ${LOCALES:S/$/.out/g}

afterinstall:
	for l in ${LOCALES}; do \
		${INSTALL} ${COPY} -m 644 -o ${BINOWN} -g ${BINGRP} $$l.out \
			   ${LOCALEDIR}/$$l/LC_TIME; \
	done
	for l in ${DE_LINKS}; do \
		ln -fs ../de_DE.ISO_8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.ISO_8859-1/LC_TIME; \
	done
	for l in ${IT_LINKS}; do \
		ln -fs ../it_IT.ISO_8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.ISO_8859-1/LC_TIME; \
	done
	for l in ${FR_LINKS}; do \
		ln -fs ../fr_FR.ISO_8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.ISO_8859-1/LC_TIME; \
	done
	for l in ${LN_LINKS}; do \
		ln -fs ../lt_LN.ISO_8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l/LC_TIME; \
	done
	for l in ${NL_LINKS}; do \
		ln -fs ../nl_NL.ISO_8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.ISO_8859-1/LC_TIME; \
	done
	for l in ${US_LINKS}; do \
		ln -fs ../en_US.ISO_8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.ISO_8859-1/LC_TIME; \
	done

.include <bsd.prog.mk>
