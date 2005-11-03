# $FreeBSD: src/sys/modules/owi/Makefile,v 1.3 2004/09/04 06:37:58 ru Exp $

.PATH: ${.CURDIR}/../../dev/owi

KMOD=	if_owi
SRCS=	opt_wi.h if_owi.c if_owi_pccard.c \
	card_if.h device_if.h bus_if.h pci_if.h pccarddevs.h

.include <bsd.kmod.mk>
