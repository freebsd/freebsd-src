# $FreeBSD$

.include "${.CURDIR}/../named/Makefile.inc"

.PATH:	${BIND_DIR}/named
.PATH:	${BIND_DIR}/man

MAN8=	named.restart.8
CLEANFILES+= named.restart

all:	named.restart

realinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    named.restart ${DESTDIR}${BINDIR}

named.restart:	named.restart.sh ${BIND_DIR}/Makefile
	sed -e "s|%INDOT%|${INDOT}|" \
	    -e "s|%DESTSBIN%|${DESTSBIN}|" \
	    < ${BIND_DIR}/named/named.restart.sh > named.restart

.include <bsd.prog.mk>
