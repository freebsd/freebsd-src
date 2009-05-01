# $FreeBSD: src/sys/modules/pst/Makefile,v 1.1.32.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/pst

KMOD=	pst
SRCS=	pst-pci.c pst-iop.c pst-raid.c
SRCS+=  device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
