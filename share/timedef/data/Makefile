# $Id: Makefile,v 1.4 1996/11/29 21:01:28 ache Exp $

NOMAN=YES
CLEANFILES+= ${LOCALES:S/$/.out/g}

LOCALES=        da_DK.ISO_8859-1 de_DE.ISO_8859-1 en_GB.ISO_8859-1 \
		pt_PT.ISO_8859-1 ru_SU.KOI8-R ru_SU.CP866
LOCALEDIR=      ${DESTDIR}/usr/share/locale

DE_LINKS =      de_AT de_CH

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

.include <bsd.prog.mk>
