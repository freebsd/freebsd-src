# $FreeBSD: src/sys/modules/pst/Makefile,v 1.1 2003/08/30 08:01:05 sos Exp $

.PATH: ${.CURDIR}/../../dev/pst

KMOD=	pst
SRCS=	pst-pci.c pst-iop.c pst-raid.c
SRCS+=  device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
