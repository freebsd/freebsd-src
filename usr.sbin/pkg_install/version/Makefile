# $FreeBSD: src/usr.sbin/pkg_install/version/Makefile,v 1.3.2.3 1999/12/03 06:18:03 billf Exp $

MAINTAINER=	billf@FreeBSD.org
MAINTAINER+=	bmah@CA.Sandia.GOV
MAN1=		pkg_version.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/pkg_version.pl ${DESTDIR}${BINDIR}/pkg_version

.include <bsd.prog.mk>
