# $FreeBSD: src/sys/modules/ciss/Makefile,v 1.1.2.2 2004/05/28 01:53:47 ps Exp $

KMOD		=  ciss
.PATH:		${.CURDIR}/../../dev/${KMOD}
SRCS		=  ciss.c
SRCS		+= opt_scsi.h opt_cam.h
SRCS		+= device_if.h bus_if.h pci_if.h

#CFLAGS		+=-DCISS_DEBUG=0

.include <bsd.kmod.mk>
