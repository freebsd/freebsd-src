# $FreeBSD: src/sys/modules/pcfclock/Makefile,v 1.4.36.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.PATH:	${.CURDIR}/../../dev/ppbus

KMOD=	pcfclock
SRCS=	pcfclock.c opt_pcfclock.h
SRCS+=	bus_if.h device_if.h ppbus_if.h

.include <bsd.kmod.mk>
