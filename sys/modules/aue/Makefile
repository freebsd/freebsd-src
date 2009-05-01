# $FreeBSD: src/sys/modules/aue/Makefile,v 1.8.8.1 2009/04/15 03:14:26 kensmith Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	if_aue
SRCS=	if_aue.c opt_usb.h device_if.h bus_if.h
SRCS+=	miibus_if.h usbdevs.h

.include <bsd.kmod.mk>
