# Makefile
# $FreeBSD: src/usr.sbin/manctl/Makefile,v 1.9.2.1 1999/08/29 15:43:39 peter Exp $

MAN8=	manctl.8

beforeinstall: 
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/manctl.sh ${DESTDIR}${BINDIR}/manctl

.include <bsd.prog.mk>
