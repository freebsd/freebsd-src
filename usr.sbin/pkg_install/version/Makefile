# $FreeBSD: src/usr.sbin/pkg_install/version/Makefile,v 1.5.2.2 2001/04/09 22:29:10 bmah Exp $

MAINTAINER=	bmah@FreeBSD.org
MAN1=		pkg_version.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/pkg_version.pl ${DESTDIR}${BINDIR}/pkg_version

test:
	./test-pkg_version.sh

.include <bsd.prog.mk>
