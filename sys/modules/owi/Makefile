# $FreeBSD: src/sys/modules/owi/Makefile,v 1.3.12.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/owi

KMOD=	if_owi
SRCS=	opt_wi.h if_owi.c if_owi_pccard.c \
	card_if.h device_if.h bus_if.h pci_if.h pccarddevs.h

.include <bsd.kmod.mk>
