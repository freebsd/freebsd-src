# $FreeBSD$

MAINTAINER=	bmah@FreeBSD.org
MAN1=		pkg_version.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/pkg_version.pl ${DESTDIR}${BINDIR}/pkg_version

test:
	./test-pkg_version.sh

.include <bsd.prog.mk>
