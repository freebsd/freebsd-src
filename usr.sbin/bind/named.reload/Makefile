# $Id$

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${BIND_DIR}/named
.PATH:	${BIND_DIR}/man

BINDIR=	${DESTSBIN}
MAN8=	named.reload.8
CLEANFILES+= named.reload

all:	named.reload

realinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} named.reload \
		${DESTDIR}${BINDIR}

named.reload:	named.reload.sh ${BIND_DIR}/Makefile
	sed -e "s|%INDOT%|${INDOT}|" \
	    -e "s|%DESTSBIN%|${DESTSBIN}|" \
	    < ${BIND_DIR}/named/named.reload.sh > named.reload

.include <bsd.prog.mk>
