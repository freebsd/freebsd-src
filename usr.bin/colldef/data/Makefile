# $Id: Makefile,v 1.1 1995/11/03 14:53:50 ache Exp $

NOMAN=YES
CLEANFILES+= ${LOCALES:S/$/.out/g}

LOCALES=        ru_SU.KOI8-R ru_SU.CP866 lt_LN.ISO_8859-1
LOCALEDIR=      ${DESTDIR}/usr/share/locale

.if exists(${.OBJDIR}/../colldef)
COLLDEF=${.OBJDIR}/../colldef
.else
COLLDEF=${.CURDIR}/../colldef
.endif

LATIN1LINKS = \
	da_DK de_AT de_CH de_DE en_AU en_CA en_GB en_US es_ES fi_FI \
	fr_BE fr_CA fr_CH fr_FR is_IS it_CH it_IT nl_BE nl_NL no_NO \
	pt_PT sv_SE

.SUFFIXES: .src .out

.src.out:
	${COLLDEF} -o ${.TARGET} ${.IMPSRC}

all: ${LOCALES:S/$/.out/g}

afterinstall:
	for l in ${LOCALES}; do \
		${INSTALL} ${COPY} -m 644 -o ${BINOWN} -g ${BINGRP} $$l.out ${LOCALEDIR}/$$l/LC_COLLATE; \
	done
	for l in ${LATIN1LINKS}; do \
		ln -fs ../lt_LN.ISO_8859-1/LC_COLLATE \
		       ${LOCALEDIR}/$$l.ISO_8859-1/LC_COLLATE; \
	done

.include <bsd.prog.mk>
