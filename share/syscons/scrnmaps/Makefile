# $FreeBSD: src/share/syscons/scrnmaps/Makefile,v 1.20 2002/07/29 09:40:14 ru Exp $

SCRMAPS = iso-8859-1_to_cp437.scm iso-8859-4_for_vga9.scm \
	  iso-8859-7_to_cp437.scm \
	  koi8-r2cp866.scm koi8-u2cp866u.scm \
	  us-ascii_to_cp437.scm

SCRMAPS_MK = ${SCRMAPS:R:S/$/.mk/g}
SCRDIR = ${SHAREDIR}/syscons/scrnmaps
CLEANFILES+= ${SCRMAPS_MK} ${SCRMAPS}

NOMAN = noman

all: ${SCRMAPS}

build-tools: ${SCRMAPS_MK}

install:
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} \
	    ${SCRMAPS} ${DESTDIR}${SCRDIR}

${SCRMAPS}: ${.TARGET:R}.mk
	./${.TARGET:R}.mk ${.TARGET:R}.tmp
	uuencode ${.TARGET:R}.tmp ${.TARGET:R} > ${.TARGET}
	rm -f ${.TARGET:R}.tmp

${SCRMAPS_MK}: ${.TARGET:R} mkscrfil.c
	${CC} -static ${CFLAGS} -I${.CURDIR} -DFIL=\"${.TARGET:R}\" ${LDFLAGS} \
	    -o ${.TARGET} ${.CURDIR}/mkscrfil.c

.include <bsd.prog.mk>
