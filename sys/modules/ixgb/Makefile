#$FreeBSD: src/sys/modules/ixgb/Makefile,v 1.3.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/ixgb

KMOD=	if_ixgb
SRCS=	if_ixgb.c ixgb_hw.c ixgb_ee.c
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
