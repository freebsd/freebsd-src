# $FreeBSD: src/sys/modules/sym/Makefile,v 1.3.30.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/sym

KMOD=	sym
SRCS=	sym_hipd.c opt_sym.h opt_cam.h opt_scsi.h
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
