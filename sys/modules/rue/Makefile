# $FreeBSD: src/sys/modules/rue/Makefile,v 1.3 2005/09/27 18:10:37 mlaier Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	if_rue
SRCS=	if_rue.c opt_usb.h device_if.h bus_if.h
SRCS+=	miibus_if.h usbdevs.h

.include <bsd.kmod.mk>
