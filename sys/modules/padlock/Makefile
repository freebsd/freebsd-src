# $FreeBSD: src/sys/modules/padlock/Makefile,v 1.3.14.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../crypto/via

KMOD=	padlock
SRCS=	padlock.c padlock_cipher.c padlock_hash.c
SRCS   += device_if.h bus_if.h opt_bus.h cryptodev_if.h

.include <bsd.kmod.mk>
