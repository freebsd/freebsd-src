# $FreeBSD: src/sys/modules/if_ndis/Makefile,v 1.7.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/if_ndis

KMOD=	if_ndis
SRCS=	if_ndis.c if_ndis_pci.c if_ndis_pccard.c if_ndis_usb.c
SRCS+=	device_if.h bus_if.h pci_if.h card_if.h pccarddevs.h
SRCS+=	opt_bus.h opt_usb.h usb_if.h usbdevs.h

.include <bsd.kmod.mk>
