# $FreeBSD: src/usr.sbin/adduser/Makefile,v 1.12.2.1 1999/08/29 15:39:00 peter Exp $

SCRIPTS= adduser.perl rmuser.perl
MAN8=	 adduser.8    rmuser.8

beforeinstall:
.for script in ${SCRIPTS}
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/${script} ${DESTDIR}${BINDIR}/${script:R}
.endfor

.include <bsd.prog.mk>
