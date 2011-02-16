# $FreeBSD: src/sys/modules/pcfclock/Makefile,v 1.4.40.1 2010/12/21 17:10:29 kensmith Exp $

.PATH:	${.CURDIR}/../../dev/ppbus

KMOD=	pcfclock
SRCS=	pcfclock.c opt_pcfclock.h
SRCS+=	bus_if.h device_if.h ppbus_if.h

.include <bsd.kmod.mk>
