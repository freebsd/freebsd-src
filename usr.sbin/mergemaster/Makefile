# $FreeBSD$

MAINTAINER=	billf@FreeBSD.org
MAN8=		mergemaster.8

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/mergemaster ${DESTDIR}${BINDIR}/mergemaster

.include <bsd.prog.mk>

