# $FreeBSD: src/sys/modules/pst/Makefile,v 1.1.2.1 2003/09/01 04:35:22 scottl Exp $

.PATH: ${.CURDIR}/../../dev/pst

KMOD=	pst
SRCS=	pst-pci.c pst-iop.c pst-raid.c
SRCS+=  device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
