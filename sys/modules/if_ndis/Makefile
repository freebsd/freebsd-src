# $FreeBSD: src/sys/modules/if_ndis/Makefile,v 1.6.10.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/if_ndis

KMOD=	if_ndis
SRCS=	if_ndis.c if_ndis_pci.c if_ndis_pccard.c if_ndis_usb.c
SRCS+=	device_if.h bus_if.h pci_if.h card_if.h pccarddevs.h
SRCS+=	opt_usb.h usbdevs.h

.include <bsd.kmod.mk>
