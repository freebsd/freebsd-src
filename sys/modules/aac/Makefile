# $FreeBSD: src/sys/modules/aac/Makefile,v 1.8.18.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/aac

.if ${MACHINE_ARCH} == "i386"
SUBDIR= aac_linux
.endif

KMOD=	aac
SRCS=	aac.c aac_pci.c aac_disk.c aac_cam.c
SRCS+=	opt_scsi.h opt_cam.h opt_aac.h
SRCS+=	device_if.h bus_if.h pci_if.h

# To enable debug output from the driver, uncomment these two lines.
#CFLAGS+= -DAAC_DEBUG=2
#SRCS+=	aac_debug.c

.include <bsd.kmod.mk>
