# $FreeBSD$

NOMAN=YES
CLEANFILES+= ${LOCALES:S/$/.out/g}

LOCALES=	en_US.ISO_8859-1 \
		nl_NL.ISO_8859-1 \
		ru_RU.KOI8-R

LOCALEDIR=      ${DESTDIR}/usr/share/locale

.SUFFIXES: .src .out

.src.out:
	grep -v '^#' < ${.IMPSRC} > ${.TARGET}

all: ${LOCALES:S/$/.out/g}

afterinstall:
	for l in ${LOCALES}; do \
		${INSTALL} ${COPY} -m 644 -o ${BINOWN} -g ${BINGRP} $$l.out \
			   ${LOCALEDIR}/$$l/LC_NUMERIC; \
	done

.include <bsd.prog.mk>
