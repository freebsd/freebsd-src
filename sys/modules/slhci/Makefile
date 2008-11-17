# $FreeBSD$

.PATH:	${.CURDIR}/../../dev/usb

KMOD=	slhci
SRCS=	sl811hs.c slhci_pccard.c

SRCS+=	bus_if.h device_if.h card_if.h power_if.h \
	usbdevs.h pccarddevs.h \
	opt_slhci.h opt_usb.h \

.include <bsd.kmod.mk>
