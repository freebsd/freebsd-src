# $FreeBSD$

.PATH: ${.CURDIR}/../../crypto/aesni

KMOD=	aesni
SRCS=	aesni.c aesni_wrap.c
SRCS+=	aesencdec_$(MACHINE_ARCH).S aeskeys_$(MACHINE_ARCH).S
SRCS+=	device_if.h bus_if.h opt_bus.h cryptodev_if.h

.include <bsd.kmod.mk>
