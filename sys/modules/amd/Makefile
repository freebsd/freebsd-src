# $FreeBSD: src/sys/modules/amd/Makefile,v 1.1.34.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/amd

KMOD=	amd
SRCS=	amd.c
SRCS+=	opt_scsi.h opt_cam.h
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
