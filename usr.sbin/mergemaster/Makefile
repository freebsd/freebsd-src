# $FreeBSD: src/usr.sbin/mergemaster/Makefile,v 1.3.2.1 1999/11/03 20:58:47 billf Exp $

MAINTAINER=	billf@FreeBSD.org
MAINTAINER+=	doug@gorean.org
MAN8=		mergemaster.8

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/mergemaster.sh ${DESTDIR}${BINDIR}/mergemaster

.include <bsd.prog.mk>

