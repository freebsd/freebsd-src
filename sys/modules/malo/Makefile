# $FreeBSD: src/sys/modules/malo/Makefile,v 1.2.4.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/malo

KMOD    = if_malo
SRCS    = if_malo.c if_malohal.c if_malo_pci.c
SRCS+=	device_if.h bus_if.h pci_if.h opt_malo.h

opt_malo.h:
	echo '#define MALO_DEBUG 1'> $@

.include <bsd.kmod.mk>
