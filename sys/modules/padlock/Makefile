# $FreeBSD: src/sys/modules/padlock/Makefile,v 1.3.12.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../crypto/via

KMOD=	padlock
SRCS=	padlock.c padlock_cipher.c padlock_hash.c
SRCS   += device_if.h bus_if.h opt_bus.h cryptodev_if.h

.include <bsd.kmod.mk>
