# $FreeBSD: src/sys/modules/mfi/mfip/Makefile,v 1.1.2.1.4.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../../dev/mfi

KMOD=	mfip
SRCS=	mfi_cam.c
SRCS+=	opt_mfi.h opt_cam.h
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
