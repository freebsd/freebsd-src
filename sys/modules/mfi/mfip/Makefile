# $FreeBSD: src/sys/modules/mfi/mfip/Makefile,v 1.1.10.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../../dev/mfi

KMOD=	mfip
SRCS=	mfi_cam.c
SRCS+=	opt_mfi.h opt_cam.h
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
