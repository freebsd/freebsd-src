# $FreeBSD: src/sys/i386/boot/dosboot/Makefile,v 1.6.2.1 2002/08/07 16:31:56 ru Exp $
#

MPROG=	fbsdboot.exe
CLEANFILES+=	${MPROG}
BINMODE=	444
NOMAN=
STRIP=

all:	${MPROG}

install:	${MPROG}
	${INSTALL} ${STRIP} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${INSTALLFLAGS} ${MPROG} ${DESTDIR}${BINDIR}


${MPROG}:	${MPROG}.uu
	uudecode < ${.CURDIR}/${MPROG}.uu

.include <bsd.prog.mk>
