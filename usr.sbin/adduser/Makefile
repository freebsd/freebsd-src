#	$Id: Makefile,v 1.11 1997/02/22 16:01:14 peter Exp $

SCRIPTS= adduser.perl rmuser.perl
MAN8=	 adduser.8    rmuser.8

beforeinstall:
.for script in ${SCRIPTS}
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/${script} ${DESTDIR}${BINDIR}/${script:R}
.endfor

.include <bsd.prog.mk>
