# $FreeBSD: src/sys/modules/xe/Makefile,v 1.2.26.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/xe

KMOD=	if_xe
SRCS=	if_xe.c if_xe_pccard.c
SRCS+=	bus_if.h card_if.h device_if.h pccarddevs.h

.include <bsd.kmod.mk>
