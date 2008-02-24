#$FreeBSD: src/sys/modules/ixgb/Makefile,v 1.3 2005/09/27 18:10:35 mlaier Exp $

.PATH: ${.CURDIR}/../../dev/ixgb

KMOD=	if_ixgb
SRCS=	if_ixgb.c ixgb_hw.c ixgb_ee.c
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
