# $FreeBSD$

NOMAN=YES
CLEANFILES+= ${LOCALES:S/$/.out/g}

LOCALES=	af_ZA.ISO_8859-1 \
		cs_CZ.ISO_8859-2 \
		da_DK.ISO_8859-1 \
		de_AT.ISO_8859-1 \
		de_DE.ISO_8859-1 \
		el_GR.ISO_8859-7 \
		en_AU.ISO_8859-1 \
		en_CA.ISO_8859-1 \
		en_GB.ISO_8859-1 \
		en_NZ.ISO_8859-1 \
		en_US.ISO_8859-1 \
		et_EE.ISO_8859-15 \
		fi_FI.ISO_8859-1 \
		fr_FR.ISO_8859-1 \
		fr_CA.ISO_8859-1 \
		hr_HR.ISO_8859-2 \
		hu_HU.ISO_8859-2 \
		is_IS.ISO_8859-1 \
		it_IT.ISO_8859-1 \
		nl_NL.ISO_8859-1 \
		no_NO.ISO_8859-1 \
		pl_PL.ISO_8859-2 \
		pt_PT.ISO_8859-1 \
		ru_RU.CP866 \
		ru_RU.ISO_8859-5 \
		ru_RU.KOI8-R \
		sl_SI.ISO_8859-2 \
		sv_SE.ISO_8859-1 \
		tr_TR.ISO_8859-9 \
		uk_UA.KOI8-U \
		ko_KR.EUC \
		ja_JP.EUC \
		zh_TW.Big5 \
		zh_CN.EUC

LOCALEDIR=      ${DESTDIR}/usr/share/locale

.SUFFIXES: .src .out

.src.out:
	grep -v '^#' < ${.IMPSRC} > ${.TARGET}

all: ${LOCALES:S/$/.out/g}

afterinstall:
.for lang in ${LOCALES}
	${INSTALL} ${COPY} -m 644 -o ${BINOWN} -g ${BINGRP} ${lang}.out \
		${LOCALEDIR}/${lang}/LC_MONETARY
.endfor

.include <bsd.prog.mk>
