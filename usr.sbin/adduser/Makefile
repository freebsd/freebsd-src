# $FreeBSD: src/usr.sbin/adduser/Makefile,v 1.14 1999/08/28 01:15:10 peter Exp $

MAINTAINER= wosch

SCRIPTS= adduser.perl rmuser.perl
MAN8=	 adduser.8    rmuser.8

beforeinstall:
.for script in ${SCRIPTS}
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/${script} ${DESTDIR}${BINDIR}/${script:R}
.endfor

.include <bsd.prog.mk>
