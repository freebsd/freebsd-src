# $FreeBSD: src/sys/modules/rue/Makefile,v 1.1.4.2 2004/03/01 00:07:25 julian Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	if_rue
SRCS=	if_rue.c opt_bdg.h opt_usb.h device_if.h bus_if.h
SRCS+=	miibus_if.h
KMODDEPS= miibus

.include <bsd.kmod.mk>
