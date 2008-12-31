
# $FreeBSD: src/sys/modules/puc/Makefile,v 1.2.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH:	${.CURDIR}/../../dev/puc

KMOD=	puc
SRCS=	puc.c puc_cfg.c puc_pci.c puc_pccard.c pucdata.c
SRCS+=	bus_if.h device_if.h serdev_if.c serdev_if.h \
	card_if.h pci_if.h

MFILES=	kern/bus_if.m kern/device_if.m kern/serdev_if.m \
	dev/pccard/card_if.m dev/pci/pci_if.m

.include <bsd.kmod.mk>
