# $FreeBSD: src/sys/modules/slhci/Makefile,v 1.2 2007/06/20 00:29:51 delphij Exp $

.PATH:	${.CURDIR}/../../dev/usb

KMOD=	slhci
SRCS=	sl811hs.c slhci_pccard.c

SRCS+= opt_slhci.h opt_usb.h device_if.h bus_if.h usbdevs.h card_if.h power_if.h pccarddevs.h

.include <bsd.kmod.mk>
