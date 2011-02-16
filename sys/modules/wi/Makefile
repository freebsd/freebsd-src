# $FreeBSD: src/sys/modules/wi/Makefile,v 1.18.2.1.2.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/wi

KMOD=	if_wi
SRCS=	if_wi.c if_wi_pccard.c if_wi_pci.c \
	card_if.h device_if.h bus_if.h pci_if.h pccarddevs.h

.include <bsd.kmod.mk>
