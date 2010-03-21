# $FreeBSD: src/sys/modules/mly/Makefile,v 1.3.38.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/mly

KMOD=	mly
SRCS=	mly.c
SRCS+=	opt_scsi.h opt_cam.h
SRCS+=	device_if.h bus_if.h pci_if.h

#CFLAGS+=	-DMLY_DEBUG=1
CFLAGS+=	-DMLY_MODULE

.include <bsd.kmod.mk>
