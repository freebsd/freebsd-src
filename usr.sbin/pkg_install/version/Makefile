# $FreeBSD: src/usr.sbin/pkg_install/version/Makefile,v 1.5.2.1 2000/08/25 19:00:50 bmah Exp $

MAINTAINER=	bmah@FreeBSD.org
MAN1=		pkg_version.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/pkg_version.pl ${DESTDIR}${BINDIR}/pkg_version

.include <bsd.prog.mk>
