# $FreeBSD: src/sys/modules/pcfclock/Makefile,v 1.4.34.1 2009/04/15 03:14:26 kensmith Exp $

.PATH:	${.CURDIR}/../../dev/ppbus

KMOD=	pcfclock
SRCS=	pcfclock.c opt_pcfclock.h
SRCS+=	bus_if.h device_if.h ppbus_if.h

.include <bsd.kmod.mk>
