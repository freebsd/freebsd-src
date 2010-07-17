# $FreeBSD: src/sys/modules/mfi/mfip/Makefile,v 1.1.12.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../../dev/mfi

KMOD=	mfip
SRCS=	mfi_cam.c
SRCS+=	opt_mfi.h opt_cam.h
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
