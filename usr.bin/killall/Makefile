# $FreeBSD: src/usr.bin/killall/Makefile,v 1.3 1999/08/28 01:02:30 peter Exp $
MAINTAINER= wosch

MAN1=	killall.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/killall.pl ${DESTDIR}${BINDIR}/killall

.include <bsd.prog.mk>
