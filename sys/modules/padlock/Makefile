# $FreeBSD: src/sys/modules/padlock/Makefile,v 1.3.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../crypto/via

KMOD=	padlock
SRCS=	padlock.c padlock_cipher.c padlock_hash.c
SRCS   += device_if.h bus_if.h opt_bus.h cryptodev_if.h

.include <bsd.kmod.mk>
