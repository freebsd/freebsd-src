# Makefile
# $FreeBSD: src/usr.sbin/manctl/Makefile,v 1.10 1999/08/28 01:16:59 peter Exp $

MAN8=	manctl.8

beforeinstall: 
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/manctl.sh ${DESTDIR}${BINDIR}/manctl

.include <bsd.prog.mk>
