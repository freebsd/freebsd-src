# $FreeBSD: src/sys/modules/sound/driver/driver/Makefile,v 1.2 2001/01/06 14:00:16 obrien Exp $

.PATH: ${.CURDIR}/../../../../dev/sound

KMOD=	snd_driver
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	driver.c

.include <bsd.kmod.mk>
