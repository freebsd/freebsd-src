# $FreeBSD$

NOMAN=YES
CLEANFILES+= ${LOCALES:S/$/.out/g}

LOCALES=        af_ZA.ISO8859-1 \
		cs_CZ.ISO8859-2 \
		da_DK.ISO8859-1 \
		de_AT.ISO8859-1 \
		de_DE.ISO8859-1 \
		el_GR.ISO8859-7 \
		en_AU.ISO8859-1 \
		en_CA.ISO8859-1 \
		en_GB.ISO8859-1 \
		en_NZ.ISO8859-1 \
		en_US.ISO8859-1 \
		es_ES.ISO8859-1 \
		et_EE.ISO8859-15 \
		fi_FI.ISO8859-1 \
		fr_FR.ISO8859-1 \
		fr_CA.ISO8859-1 \
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

ASCIILINKS = en_AU en_CA en_GB en_NZ en_US

.SUFFIXES: .src .out

.src.out:
	grep -v '^#' < ${.IMPSRC} > ${.TARGET}

all: ${LOCALES:S/$/.out/g}

afterinstall:
.for lang in ${LOCALES}
	${INSTALL} ${COPY} -m 644 -o ${BINOWN} -g ${BINGRP} ${lang}.out \
		${LOCALEDIR}/${lang}/LC_MONETARY
.endfor
.for link in ${ASCIILINKS}
	ln -fs ../${link}.ISO8859-1/LC_MONETARY \
		${LOCALEDIR}/${link}.US-ASCII/LC_MONETARY
.endfor

.include <bsd.prog.mk>
