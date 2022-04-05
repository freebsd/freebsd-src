# $FreeBSD$

.PATH: ${SRCTOP}/sys/dev/hid

KMOD=	bcm5974
SRCS=	bcm5974.c
SRCS+=	opt_hid.h opt_usb.h
SRCS+=	bus_if.h device_if.h usbdevs.h

.include <bsd.kmod.mk>
