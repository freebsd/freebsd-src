# $FreeBSD: src/sys/modules/axe/Makefile,v 1.2.6.1 2004/12/08 22:33:11 rsm Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	if_axe
SRCS=	if_axe.c opt_bdg.h opt_usb.h device_if.h bus_if.h
SRCS+=	miibus_if.h
KMODDEPS= miibus

.include <bsd.kmod.mk>
