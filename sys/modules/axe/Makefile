# $FreeBSD: src/sys/modules/axe/Makefile,v 1.1 2003/04/20 19:05:32 wpaul Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	if_axe
SRCS=	if_axe.c opt_bdg.h opt_usb.h device_if.h bus_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
