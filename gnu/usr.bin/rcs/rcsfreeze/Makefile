# $FreeBSD$

MAN=	rcsfreeze.1

afterinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/rcsfreeze.sh ${DESTDIR}${BINDIR}/rcsfreeze

.include "../../Makefile.inc"
.include <bsd.prog.mk>
