# $FreeBSD: src/share/syscons/scrnmaps/Makefile,v 1.14 2000/01/08 18:11:06 ache Exp $

SCRMAPS = koi8-r2cp866.scm iso-8859-1_to_cp437.scm iso-8859-4_to_cp437.scm

SCRMAPS_MK = ${SCRMAPS:R:S/$/.mk/g}
SCRDIR = ${SHAREDIR}/syscons/scrnmaps
CLEANFILES+= ${SCRMAPS_MK} ${SCRMAPS}

NOMAN = noman

all: ${SCRMAPS}

build-tools: ${SCRMAPS_MK}

install:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} \
	    ${SCRMAPS} ${DESTDIR}${SCRDIR}

${SCRMAPS}: ${.TARGET:R}.mk
	./${.TARGET:R}.mk ${.TARGET:R}.tmp
	uuencode ${.TARGET:R}.tmp ${.TARGET:R} > ${.TARGET}
	rm -f ${.TARGET:R}.tmp

${SCRMAPS_MK}: ${.TARGET:R} mkscrfil.c
	${CC} -static ${CFLAGS} -I${.CURDIR} -DFIL=\"${.TARGET:R}\" ${LDFLAGS} \
	    -o ${.TARGET} ${.CURDIR}/mkscrfil.c

.include <bsd.prog.mk>
