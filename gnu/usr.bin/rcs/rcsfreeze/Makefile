# $FreeBSD: src/gnu/usr.bin/rcs/rcsfreeze/Makefile,v 1.5.8.3 2002/08/07 16:31:45 ru Exp $

MAN=	rcsfreeze.1

afterinstall:
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/rcsfreeze.sh ${DESTDIR}${BINDIR}/rcsfreeze

.include "../../Makefile.inc"
.include <bsd.prog.mk>
