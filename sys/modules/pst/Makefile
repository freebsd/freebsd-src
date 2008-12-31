# $FreeBSD: src/sys/modules/pst/Makefile,v 1.1.30.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/pst

KMOD=	pst
SRCS=	pst-pci.c pst-iop.c pst-raid.c
SRCS+=  device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
