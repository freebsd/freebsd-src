# $FreeBSD: src/sys/modules/if_ndis/Makefile,v 1.4 2004/05/26 00:53:04 imp Exp $

.PATH: ${.CURDIR}/../../dev/if_ndis

KMOD=	if_ndis
SRCS=	if_ndis.c if_ndis_pci.c if_ndis_pccard.c
SRCS+=	opt_bdg.h device_if.h bus_if.h pci_if.h card_if.h pccarddevs.h

.include <bsd.kmod.mk>
