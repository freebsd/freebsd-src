# Makefile for periodic(8)
#
# $FreeBSD: src/usr.sbin/periodic/Makefile,v 1.1.1.1.2.1 1999/08/29 15:45:04 peter Exp $

MAN8=	periodic.8

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/periodic.sh ${DESTDIR}${BINDIR}/periodic

.include <bsd.prog.mk>
