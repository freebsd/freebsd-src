# $FreeBSD$

NOMAN=YES
CLEANFILES+= ${LOCALES:S/$/.out/g}

LOCALES=	af_ZA.ISO_8859-1 \
		da_DK.ISO_8859-1 \
		de_DE.ISO_8859-1 \
		en_US.ISO_8859-1 \
		fi_FI.ISO_8859-1 \
		fr_FR.ISO_8859-1 \
		is_IS.ISO_8859-1 \
		nl_NL.ISO_8859-1 \
		no_NO.ISO_8859-1 \
		ru_RU.KOI8-R \
		sv_SE.ISO_8859-1

LOCALEDIR=      ${DESTDIR}/usr/share/locale

.SUFFIXES: .src .out

.src.out:
	grep -v '^#' < ${.IMPSRC} > ${.TARGET}

all: ${LOCALES:S/$/.out/g}

afterinstall:
	for l in ${LOCALES}; do \
		${INSTALL} ${COPY} -m 644 -o ${BINOWN} -g ${BINGRP} $$l.out \
			   ${LOCALEDIR}/$$l/LC_MONETARY; \
	done

.include <bsd.prog.mk>
