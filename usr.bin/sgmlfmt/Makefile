#	$Id: Makefile,v 1.1.1.1 1995/05/09 23:58:06 jfieber Exp $

MAN1=	sgmlfmt.1

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/sgmlfmt.pl ${DESTDIR}${BINDIR}/sgmlfmt

.include <bsd.prog.mk>
