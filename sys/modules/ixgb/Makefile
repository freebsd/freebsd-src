#$FreeBSD: src/sys/modules/ixgb/Makefile,v 1.3.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/ixgb

KMOD=	if_ixgb
SRCS=	if_ixgb.c ixgb_hw.c ixgb_ee.c
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
