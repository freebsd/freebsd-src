#	$Id: Makefile,v 1.2 1995/07/25 00:36:45 bde Exp $

MAN1=	sgmlfmt.1

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/sgmlfmt.pl ${DESTDIR}${BINDIR}/sgmlfmt

.include <bsd.prog.mk>
