# $FreeBSD: src/gnu/usr.bin/rcs/rcsfreeze/Makefile,v 1.5.8.1 2001/04/25 12:03:55 ru Exp $

MAN=	rcsfreeze.1

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/rcsfreeze.sh ${DESTDIR}${BINDIR}/rcsfreeze

.include "../../Makefile.inc"
.include <bsd.prog.mk>
