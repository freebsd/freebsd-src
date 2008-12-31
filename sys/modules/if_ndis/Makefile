# $FreeBSD: src/sys/modules/if_ndis/Makefile,v 1.6.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/if_ndis

KMOD=	if_ndis
SRCS=	if_ndis.c if_ndis_pci.c if_ndis_pccard.c if_ndis_usb.c
SRCS+=	device_if.h bus_if.h pci_if.h card_if.h pccarddevs.h
SRCS+=	opt_usb.h usbdevs.h

.include <bsd.kmod.mk>
