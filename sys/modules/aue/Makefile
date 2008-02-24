# $FreeBSD: src/sys/modules/aue/Makefile,v 1.8 2005/09/27 18:10:32 mlaier Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	if_aue
SRCS=	if_aue.c opt_usb.h device_if.h bus_if.h
SRCS+=	miibus_if.h usbdevs.h

.include <bsd.kmod.mk>
