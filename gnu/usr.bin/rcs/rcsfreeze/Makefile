# $FreeBSD: src/gnu/usr.bin/rcs/rcsfreeze/Makefile,v 1.8 2002/07/29 09:40:09 ru Exp $

MAN=	rcsfreeze.1

afterinstall:
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/rcsfreeze.sh ${DESTDIR}${BINDIR}/rcsfreeze

.include "../../Makefile.inc"
.include <bsd.prog.mk>
