#	$Id: Makefile,v 1.9 1996/11/17 03:51:26 wosch Exp $

SCRIPTS= adduser.perl rmuser.perl addgroup.tcl rmgroup.sh
MAN8=	 adduser.8    rmuser.8    addgroup.8   rmgroup.8

beforeinstall:
.for script in ${SCRIPTS}
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/${script} ${DESTDIR}${BINDIR}/${script:R}
.endfor

.include <bsd.prog.mk>
