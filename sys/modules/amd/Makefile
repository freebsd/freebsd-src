# $FreeBSD: src/sys/modules/amd/Makefile,v 1.1.30.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/amd

KMOD=	amd
SRCS=	amd.c
SRCS+=	opt_scsi.h opt_cam.h
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
