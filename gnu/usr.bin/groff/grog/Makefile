# $FreeBSD$

MAN1=	grog.1
CLEANFILES= grog ${MAN1}

all: grog

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} grog \
	    ${DESTDIR}${BINDIR}

.include "../Makefile.cfg"
.include <bsd.prog.mk>

grog: grog.sh
	sed -e 's/@g@/$(g)/g' ${.ALLSRC} > ${.TARGET}
