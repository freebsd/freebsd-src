# $FreeBSD: src/usr.sbin/pkg_install/update/Makefile,v 1.1 2000/12/28 18:47:22 paul Exp $

MAN1=		pkg_update.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/pkg_update.pl ${DESTDIR}${BINDIR}/pkg_update

.include <bsd.prog.mk>
