# $FreeBSD: src/usr.sbin/mergemaster/Makefile,v 1.3.4.1 2000/11/01 08:34:30 dougb Exp $

MAINTAINER=	dougb@FreeBSD.org
MAN8=		mergemaster.8

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/mergemaster.sh ${DESTDIR}${BINDIR}/mergemaster

.include <bsd.prog.mk>

