# $FreeBSD: src/sys/modules/aue/Makefile,v 1.2.2.1 2004/03/01 00:07:24 julian Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	if_aue
SRCS=	if_aue.c opt_bdg.h opt_usb.h device_if.h bus_if.h
SRCS+=	miibus_if.h
KMODDEPS= miibus

.include <bsd.kmod.mk>
