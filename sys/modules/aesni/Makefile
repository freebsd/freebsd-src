# $FreeBSD: src/sys/modules/aesni/Makefile,v 1.1.2.2.2.1 2010/12/21 17:09:25 kensmith Exp $

.PATH: ${.CURDIR}/../../crypto/aesni

KMOD=	aesni
SRCS=	aesni.c aesni_wrap.c
SRCS+=	aesencdec_$(MACHINE_ARCH).S aeskeys_$(MACHINE_ARCH).S
SRCS+=	device_if.h bus_if.h opt_bus.h cryptodev_if.h

.include <bsd.kmod.mk>
