#	$Id: Makefile,v 1.12 1997/07/05 19:12:43 pst Exp $

MAINTAINER= wosch

SCRIPTS= adduser.perl rmuser.perl
MAN8=	 adduser.8    rmuser.8

beforeinstall:
.for script in ${SCRIPTS}
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/${script} ${DESTDIR}${BINDIR}/${script:R}
.endfor

.include <bsd.prog.mk>
