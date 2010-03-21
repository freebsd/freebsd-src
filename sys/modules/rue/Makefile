# $FreeBSD: src/sys/modules/rue/Makefile,v 1.3.10.1 2010/02/10 00:26:20 kensmith Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	if_rue
SRCS=	if_rue.c opt_usb.h device_if.h bus_if.h
SRCS+=	miibus_if.h usbdevs.h

.include <bsd.kmod.mk>
