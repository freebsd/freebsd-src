# $FreeBSD: src/sys/modules/aac/Makefile,v 1.2.2.1 2001/01/09 01:04:00 scottl Exp $

.PATH: ${.CURDIR}/../../dev/aac

KMOD=	aac
SRCS=	aac.c aac_pci.c aac_disk.c
SRCS+=	device_if.h bus_if.h pci_if.h

CFLAGS+= -DAAC_COMPAT_LINUX

#CFLAGS+= -DAAC_DEBUG=2
#SRCS+=	aac_debug.c

.include <bsd.kmod.mk>
