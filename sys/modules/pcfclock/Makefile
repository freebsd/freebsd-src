# $FreeBSD: src/sys/modules/pcfclock/Makefile,v 1.4 2001/01/06 14:00:10 obrien Exp $

.PATH:	${.CURDIR}/../../dev/ppbus

KMOD=	pcfclock
SRCS=	pcfclock.c opt_pcfclock.h
SRCS+=	bus_if.h device_if.h ppbus_if.h

.include <bsd.kmod.mk>
