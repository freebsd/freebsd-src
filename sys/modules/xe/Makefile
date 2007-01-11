# $FreeBSD: src/sys/modules/xe/Makefile,v 1.2 2004/05/26 00:53:09 imp Exp $

.PATH: ${.CURDIR}/../../dev/xe

KMOD=	if_xe
SRCS=	if_xe.c if_xe_pccard.c
SRCS+=	bus_if.h card_if.h device_if.h pccarddevs.h

.include <bsd.kmod.mk>
