#$FreeBSD: src/sys/modules/ixgb/Makefile,v 1.2 2004/06/09 08:27:05 obrien Exp $

.PATH: ${.CURDIR}/../../dev/ixgb

KMOD=	if_ixgb
SRCS=	if_ixgb.c ixgb_hw.c ixgb_ee.c
SRCS+=	device_if.h bus_if.h pci_if.h opt_bdg.h

.include <bsd.kmod.mk>
