#	$Id: Makefile,v 1.1.1.1.4.1 1995/09/17 23:40:07 davidg Exp $

MAN1=	sgmlfmt.1

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/sgmlfmt.pl ${DESTDIR}${BINDIR}/sgmlfmt

.include <bsd.prog.mk>
