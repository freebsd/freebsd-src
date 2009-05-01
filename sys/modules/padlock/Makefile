# $FreeBSD: src/sys/modules/padlock/Makefile,v 1.3.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../crypto/via

KMOD=	padlock
SRCS=	padlock.c padlock_cipher.c padlock_hash.c
SRCS   += device_if.h bus_if.h opt_bus.h cryptodev_if.h

.include <bsd.kmod.mk>
