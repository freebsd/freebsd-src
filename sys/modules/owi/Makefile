# $FreeBSD: src/sys/modules/owi/Makefile,v 1.2 2004/05/26 00:53:06 imp Exp $

.PATH: ${.CURDIR}/../../dev/owi

KMOD=	if_owi
SRCS=	opt_wi.h if_owi.c if_owi_pccard.c \
	card_if.h device_if.h bus_if.h pci_if.h pccarddevs.h

opt_wi.h:
	touch ${.TARGET}

.include <bsd.kmod.mk>
