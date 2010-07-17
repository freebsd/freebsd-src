# $FreeBSD: src/sys/modules/pst/Makefile,v 1.1.34.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/pst

KMOD=	pst
SRCS=	pst-pci.c pst-iop.c pst-raid.c
SRCS+=  device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
