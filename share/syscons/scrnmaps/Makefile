#	$Id: Makefile,v 1.10 1998/01/03 14:09:21 wosch Exp $

SCRMAPS = koi8-r2cp866.scm iso-8859-1_to_cp437.scm

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
	${CC} ${CFLAGS} -I${.CURDIR} -DFIL=\"${.TARGET:R}\" ${LDFLAGS} \
	    -o ${.TARGET} ${.CURDIR}/mkscrfil.c

.include <bsd.prog.mk>
