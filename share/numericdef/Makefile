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
		is_IS.ISO_8859-1 \
		nl_NL.ISO_8859-1 \
		no_NO.ISO_8859-1 \
		ru_RU.KOI8-R \
		sv_SE.ISO_8859-1

LOCALEDIR=      ${DESTDIR}/usr/share/locale
USENGLISH=	en_US.ISO_8859-1
USENGLISHLINKS=	en_CA.ISO_8859-1 en_GB.ISO_8859-1
GBENGLISH=	en_GB.ISO_8859-1
GBENGLISHLINKS=	en_AU.ISO_8859-1 en_NZ.ISO_8859-1
FRENCH=		fr_FR.ISO_8859-1
FRENCHLINKS=	fr_CA.ISO_8859-1 fr_CH.ISO_8859-1

.SUFFIXES: .src .out

.src.out:
	grep -v '^#' < ${.IMPSRC} > ${.TARGET}

all: ${LOCALES:S/$/.out/g}

afterinstall:
.for lang in ${LOCALES}
	${INSTALL} ${COPY} -m 644 -o ${BINOWN} -g ${BINGRP} ${lang}.out \
			   ${LOCALEDIR}/${lang}/LC_NUMERIC
.endfor
.for link in ${USENGLISHLINKS}
	ln -sf ../${USENGLISH}/LC_NUMERIC ${LOCALEDIR}/${link}/LC_NUMERIC
.endfor
.for link in ${GBENGLISHLINKS}
	ln -sf ../${GBENGLISH}/LC_NUMERIC ${LOCALEDIR}/${link}/LC_NUMERIC
.endfor
.for link in ${FRENCHLINKS}
	ln -sf ../${FRENCH}/LC_NUMERIC ${LOCALEDIR}/${link}/LC_NUMERIC
.endfor

.include <bsd.prog.mk>
