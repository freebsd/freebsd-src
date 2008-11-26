# $FreeBSD: src/sys/modules/pst/Makefile,v 1.1.28.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/pst

KMOD=	pst
SRCS=	pst-pci.c pst-iop.c pst-raid.c
SRCS+=  device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
