# $FreeBSD: src/sys/modules/mly/Makefile,v 1.3.32.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/mly

KMOD=	mly
SRCS=	mly.c
SRCS+=	opt_scsi.h opt_cam.h
SRCS+=	device_if.h bus_if.h pci_if.h

#CFLAGS+=	-DMLY_DEBUG=1
CFLAGS+=	-DMLY_MODULE

.include <bsd.kmod.mk>
