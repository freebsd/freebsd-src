# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/aac

KMOD=	aac
SRCS=	aac.c aac_pci.c aac_disk.c aac_cam.c
SRCS+=	opt_scsi.h opt_cam.h opt_aac.h
SRCS+=	device_if.h bus_if.h pci_if.h

CFLAGS+= -DAAC_COMPAT_LINUX
KMODDEPS= linux

#CFLAGS+= -DAAC_DEBUG=2
#SRCS+=	aac_debug.c

.include <bsd.kmod.mk>
