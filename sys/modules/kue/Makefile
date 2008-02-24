# $FreeBSD: src/sys/modules/kue/Makefile,v 1.7 2005/09/27 18:10:36 mlaier Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	if_kue
SRCS=	if_kue.c opt_usb.h device_if.h bus_if.h
SRCS+=	miibus_if.h usbdevs.h

.include <bsd.kmod.mk>
