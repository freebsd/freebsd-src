#$FreeBSD: src/sys/modules/ixgb/Makefile,v 1.2.22.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/ixgb

KMOD=	if_ixgb
SRCS=	if_ixgb.c ixgb_hw.c ixgb_ee.c
SRCS+=	device_if.h bus_if.h pci_if.h opt_bdg.h

.include <bsd.kmod.mk>
