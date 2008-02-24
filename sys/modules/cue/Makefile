# $FreeBSD: src/sys/modules/cue/Makefile,v 1.7 2005/09/27 18:10:33 mlaier Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	if_cue
SRCS=	if_cue.c opt_usb.h device_if.h bus_if.h
SRCS+=	miibus_if.h usbdevs.h

.include <bsd.kmod.mk>
