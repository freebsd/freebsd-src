# $FreeBSD: src/sys/modules/xe/Makefile,v 1.2.24.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/xe

KMOD=	if_xe
SRCS=	if_xe.c if_xe_pccard.c
SRCS+=	bus_if.h card_if.h device_if.h pccarddevs.h

.include <bsd.kmod.mk>
