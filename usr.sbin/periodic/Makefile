# Makefile for periodic(8)
#
# $FreeBSD: src/usr.sbin/periodic/Makefile,v 1.2 1999/08/28 01:17:51 peter Exp $

MAN8=	periodic.8

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/periodic.sh ${DESTDIR}${BINDIR}/periodic

.include <bsd.prog.mk>
