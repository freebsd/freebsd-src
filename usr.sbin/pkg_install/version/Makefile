# $FreeBSD$

MAINTAINER=	billf@FreeBSD.org
MAN8=		pkg_version.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/pkg_version.pl ${DESTDIR}${BINDIR}/pkg_version

.include <bsd.prog.mk>
