# $Id: Makefile,v 1.2.2.2 1996/12/03 10:39:27 phk Exp $

NOMAN=YES
CLEANFILES+= ${LOCALES:S/$/.out/g}

LOCALES=        da_DK.ISO_8859-1 de_DE.ISO_8859-1 en_GB.ISO_8859-1 \
		fr_FR.ISO_8859-1 it_IT.ISO_8859-1 pt_PT.ISO_8859-1 \
		ru_SU.KOI8-R ru_SU.CP866
LOCALEDIR=      ${DESTDIR}/usr/share/locale

DE_LINKS =      de_AT de_CH
FR_LINKS =      fr_BE fr_CA fr_CH
IT_LINKS =      it_CH

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
	for l in ${FR_LINKS}; do \
		ln -fs ../fr_FR.ISO_8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.ISO_8859-1/LC_TIME; \
	done
	for l in ${IT_LINKS}; do \
		ln -fs ../it_IT.ISO_8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.ISO_8859-1/LC_TIME; \
	done

.include <bsd.prog.mk>
