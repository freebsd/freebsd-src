# $FreeBSD: src/usr.sbin/named.restart/Makefile,v 1.6 1999/08/28 01:17:24 peter Exp $

.include "${.CURDIR}/../named/Makefile.inc"

MAN8=	named.restart.8
CLEANFILES+= named.restart

all:	named.restart

realinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    named.restart ${DESTDIR}${BINDIR}

named.restart:	named.restart.sh ${BIND_DIR}/Makefile
	sed -e "s|%INDOT%|${INDOT}|" \
	    -e "s|%DESTSBIN%|${DESTSBIN}|" \
	    < ${.CURDIR}/named.restart.sh > named.restart

.include <bsd.prog.mk>
