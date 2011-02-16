# $FreeBSD: src/sys/modules/sym/Makefile,v 1.3.40.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/sym

KMOD=	sym
SRCS=	sym_hipd.c opt_sym.h opt_cam.h opt_scsi.h
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
