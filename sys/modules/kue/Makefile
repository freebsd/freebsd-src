# $FreeBSD: src/sys/modules/kue/Makefile,v 1.2.2.1 2004/03/01 00:07:25 julian Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	if_kue
SRCS=	if_kue.c opt_bdg.h opt_usb.h device_if.h bus_if.h
SRCS+=	miibus_if.h
KMODDEPS= miibus

.include <bsd.kmod.mk>
