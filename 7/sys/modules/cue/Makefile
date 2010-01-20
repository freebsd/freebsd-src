# $FreeBSD$

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	if_cue
SRCS=	if_cue.c opt_usb.h device_if.h bus_if.h
SRCS+=	miibus_if.h usbdevs.h

.include <bsd.kmod.mk>
