# $FreeBSD$

.PATH:		${.CURDIR}/../../dev/mly
KMOD		=  mly
SRCS		=  mly.c mly_pci.c mly_cam.c
SRCS		+= opt_scsi.h opt_cam.h
SRCS		+= device_if.h bus_if.h pci_if.h

#CFLAGS+=	-DMLY_DEBUG=1

.include <bsd.kmod.mk>
