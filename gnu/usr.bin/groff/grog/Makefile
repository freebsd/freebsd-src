# $FreeBSD$

MAN1=	grog.1
CLEANFILES= grog ${MAN1}

all: grog

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} grog \
	    ${DESTDIR}${BINDIR}

.include "../Makefile.cfg"
.include <bsd.prog.mk>

grog: ${DIST_DIR}/grog.sh
	sed -e 's/gsoelim/soelim/' ${.ALLSRC} > ${.TARGET}
