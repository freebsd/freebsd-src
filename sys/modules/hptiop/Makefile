# $FreeBSD: src/sys/modules/hptiop/Makefile,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/hptiop

KMOD=	hptiop
SRCS=	hptiop.c
SRCS+=	opt_scsi.h opt_cam.h
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
