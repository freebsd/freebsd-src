# $FreeBSD: src/sys/modules/pcfclock/Makefile,v 1.1 2000/02/29 10:57:42 sheldonh Exp $

.PATH:	${.CURDIR}/../../dev/ppbus
KMOD=	pcfclock
SRCS=	pcfclock.c opt_pcfclock.h
SRCS+=	bus_if.h device_if.h ppbus_if.h

.include <bsd.kmod.mk>
