# $FreeBSD: src/sys/modules/padlock/Makefile,v 1.3.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../crypto/via

KMOD=	padlock
SRCS=	padlock.c padlock_cipher.c padlock_hash.c
SRCS   += device_if.h bus_if.h opt_bus.h cryptodev_if.h

.include <bsd.kmod.mk>
