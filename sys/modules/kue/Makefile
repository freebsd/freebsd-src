# $FreeBSD$

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	if_kue
SRCS=	if_kue.c opt_bdg.h opt_usb.h device_if.h bus_if.h
SRCS+=	miibus_if.h usbdevs.h

.include <bsd.kmod.mk>
