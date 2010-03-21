# $FreeBSD: src/sys/modules/xe/Makefile,v 1.2.32.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/xe

KMOD=	if_xe
SRCS=	if_xe.c if_xe_pccard.c
SRCS+=	bus_if.h card_if.h device_if.h pccarddevs.h

.include <bsd.kmod.mk>
