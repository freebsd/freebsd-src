# $FreeBSD: src/sys/i386/boot/dosboot/Makefile,v 1.6 1999/08/28 00:43:19 peter Exp $
#

MPROG=	fbsdboot.exe
CLEANFILES+=	${MPROG}
BINMODE=	444
NOMAN=
STRIP=

all:	${MPROG}

install:	${MPROG}
	${INSTALL} ${COPY} ${STRIP} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${INSTALLFLAGS} ${MPROG} ${DESTDIR}${BINDIR}


${MPROG}:	${MPROG}.uu
	uudecode < ${.CURDIR}/${MPROG}.uu

.include <bsd.prog.mk>
