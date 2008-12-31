# $FreeBSD: src/sys/modules/pcfclock/Makefile,v 1.4.32.1 2008/11/25 02:59:29 kensmith Exp $

.PATH:	${.CURDIR}/../../dev/ppbus

KMOD=	pcfclock
SRCS=	pcfclock.c opt_pcfclock.h
SRCS+=	bus_if.h device_if.h ppbus_if.h

.include <bsd.kmod.mk>
