# Makefile for grog
#
# $FreeBSD: src/gnu/usr.bin/groff/grog/Makefile,v 1.1 2000/01/12 10:30:27 asmodai Exp $

MAN1=		grog.1

MANDEPEND=	${MAN1}
CLEANFILES+=	${MANDEPEND}

all: grog

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} grog \
	    ${DESTDIR}${BINDIR}

.include <../Makefile.cfg>
.include <bsd.prog.mk>

grog: ${DIST_DIR}/grog.sh
	sed -e 's/gsoelim/soelim/' ${.ALLSRC} > ${.TARGET}
