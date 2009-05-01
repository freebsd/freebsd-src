# $FreeBSD: src/sys/modules/hptiop/Makefile,v 1.1.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/hptiop

KMOD=	hptiop
SRCS=	hptiop.c
SRCS+=	opt_scsi.h opt_cam.h
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
