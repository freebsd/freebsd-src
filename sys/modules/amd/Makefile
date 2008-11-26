# $FreeBSD: src/sys/modules/amd/Makefile,v 1.1.28.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/amd

KMOD=	amd
SRCS=	amd.c
SRCS+=	opt_scsi.h opt_cam.h
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
