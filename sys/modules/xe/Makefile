# $FreeBSD: src/sys/modules/xe/Makefile,v 1.1 2002/02/20 15:00:34 shiba Exp $

.PATH: ${.CURDIR}/../../dev/xe

KMOD=	if_xe
SRCS=	if_xe.c if_xe_pccard.c
SRCS+=	bus_if.h card_if.h device_if.h

.include <bsd.kmod.mk>
