# $FreeBSD: src/sys/modules/hptiop/Makefile,v 1.1.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/hptiop

KMOD=	hptiop
SRCS=	hptiop.c
SRCS+=	opt_scsi.h opt_cam.h
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
