#	$Id: Makefile,v 1.7.6.3 1998/01/02 13:35:52 steve Exp $

SCRIPTS= adduser.perl rmuser.perl
MAN8=	 adduser.8    rmuser.8

beforeinstall:
.for script in ${SCRIPTS}
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/${script} ${DESTDIR}${BINDIR}/${script:R}
.endfor

.include <bsd.prog.mk>
