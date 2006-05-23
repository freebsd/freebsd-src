# $FreeBSD: src/sys/modules/arcmsr/Makefile,v 1.1.2.1 2005/04/02 16:14:54 scottl Exp $

.PATH: ${.CURDIR}/../../dev/arcmsr

KMOD=	arcmsr
SRCS=	arcmsr.c
SRCS+=	opt_scsi.h opt_cam.h
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
