#	$Id: Makefile,v 1.1 1994/10/22 04:51:39 nate Exp $

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/cvsinit.sh ${DESTDIR}${BINDIR}/cvsinit

.include "../../Makefile.inc"
.include <bsd.prog.mk>
