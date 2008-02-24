# $FreeBSD: src/sys/modules/udav/Makefile,v 1.3 2005/09/27 18:10:40 mlaier Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb
KMOD=	if_udav
SRCS=	if_udav.c opt_usb.h device_if.h bus_if.h opt_inet.h
SRCS+=	miibus_if.h usbdevs.h

.include <bsd.kmod.mk>
