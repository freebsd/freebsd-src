# $FreeBSD$
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
