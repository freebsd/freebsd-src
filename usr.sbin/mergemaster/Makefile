# $FreeBSD$

MAINTAINER=	dougb@FreeBSD.org
MAN=		mergemaster.8

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/mergemaster.sh ${DESTDIR}${BINDIR}/mergemaster

.include <bsd.prog.mk>

