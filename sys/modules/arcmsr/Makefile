# $FreeBSD: src/sys/modules/arcmsr/Makefile,v 1.1.24.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/arcmsr

KMOD=	arcmsr
SRCS=	arcmsr.c
SRCS+=	opt_scsi.h opt_cam.h
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
