# $FreeBSD: src/usr.sbin/mergemaster/Makefile,v 1.3 1999/11/01 04:29:04 chris Exp $

MAINTAINER=	billf@FreeBSD.org
MAINTAINER+=	doug@gorean.org
MAN8=		mergemaster.8

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/mergemaster.sh ${DESTDIR}${BINDIR}/mergemaster

.include <bsd.prog.mk>

