# $FreeBSD: src/sys/modules/aac/Makefile,v 1.2.2.4 2003/03/28 19:51:37 scottl Exp $

.PATH: ${.CURDIR}/../../dev/aac

SUBDIR= aac_linux

KMOD=	aac
SRCS=	aac.c aac_pci.c aac_disk.c aac_cam.c
SRCS+=	opt_scsi.h opt_cam.h opt_aac.h
SRCS+=	device_if.h bus_if.h pci_if.h

#CFLAGS+= -DAAC_DEBUG=2
#SRCS+=	aac_debug.c

.include <bsd.kmod.mk>
