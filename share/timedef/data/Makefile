# $Id: Makefile,v 1.1.1.1 1995/08/06 11:15:54 ache Exp $

NOMAN=YES
CLEANFILES+= ${LOCALES:S/$/.out/g}

LOCALES=        ru_SU.KOI8-R ru_SU.CP866
LOCALEDIR=      ${DESTDIR}/usr/share/locale

.SUFFIXES: .src .out

.src.out:
	grep -v '^#' < ${.IMPSRC} > ${.TARGET}

all: ${LOCALES:S/$/.out/g}

afterinstall:
	for l in ${LOCALES}; do \
		${INSTALL} ${COPY} -m 644 -o ${BINOWN} -g ${BINGRP} $$l.out ${LOCALEDIR}/$$l/LC_TIME; \
	done

.include <bsd.prog.mk>
