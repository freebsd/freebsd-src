#	$FreeBSD$

MAN1=	sgmlfmt.1

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/sgmlfmt.pl ${DESTDIR}${BINDIR}/sgmlfmt

.include <bsd.prog.mk>
