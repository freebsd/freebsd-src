# $FreeBSD$

NOMAN=YES
CLEANFILES+= ${LOCALES:S/$/.out/g}

LOCALES=	af_ZA.ISO_8859-1 \
		cs_CZ.ISO_8859-2 \
		da_DK.ISO_8859-1 \
		de_DE.ISO_8859-1 \
		el_GR.ISO_8859-7 \
		en_US.ISO_8859-1 \
		fi_FI.ISO_8859-1 \
		fr_FR.ISO_8859-1 \
		hu_HU.ISO_8859-2 \
		is_IS.ISO_8859-1 \
		nl_NL.ISO_8859-1 \
		no_NO.ISO_8859-1 \
		pl_PL.ISO_8859-2 \
		ru_RU.CP866 \
		ru_RU.ISO_8859-5 \
		ru_RU.KOI8-R \
		sv_SE.ISO_8859-1 \
		uk_UA.KOI8-U \
		ko_KR.EUC \
		ja_JP.EUC \
		zh_TW.Big5 \
		zh_CN.EUC

ENGLISH=	en_US.ISO_8859-1
ENGLISHLINKS=	en_AU.ISO_8859-1 en_CA.ISO_8859-1 en_GB.ISO_8859-1 \
		en_NZ.ISO_8859-1
FRENCH=		fr_FR.ISO_8859-1
FRENCHLINKS=	fr_CA.ISO_8859-1 fr_CH.ISO_8859-1

LOCALEDIR=      ${DESTDIR}/usr/share/locale

.SUFFIXES: .src .out

.src.out:
	grep -v '^#' < ${.IMPSRC} > ${.TARGET}

all: ${LOCALES:S/$/.out/g}

afterinstall:
.for lang in ${LOCALES}
	${INSTALL} ${COPY} -m 644 -o ${BINOWN} -g ${BINGRP} ${lang}.out \
			   ${LOCALEDIR}/${lang}/LC_MESSAGES
.endfor
.for link in ${ENGLISHLINKS}
	ln -sf ../${ENGLISH}/LC_MESSAGES ${LOCALEDIR}/${link}/LC_MESSAGES
.endfor
.for link in ${FRENCHLINKS}
	ln -sf ../${FRENCH}/LC_MESSAGES ${LOCALEDIR}/${link}/LC_MESSAGES
.endfor

.include <bsd.prog.mk>
