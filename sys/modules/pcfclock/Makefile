# $FreeBSD: src/sys/modules/pcfclock/Makefile,v 1.4.38.1 2010/02/10 00:26:20 kensmith Exp $

.PATH:	${.CURDIR}/../../dev/ppbus

KMOD=	pcfclock
SRCS=	pcfclock.c opt_pcfclock.h
SRCS+=	bus_if.h device_if.h ppbus_if.h

.include <bsd.kmod.mk>
