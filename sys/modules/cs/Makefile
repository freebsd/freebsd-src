# $FreeBSD: src/sys/modules/cs/Makefile,v 1.3.16.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/cs

KMOD=	if_cs
SRCS=	if_cs.c if_cs_isa.c if_cs_pccard.c
SRCS+=	bus_if.h card_if.h device_if.h isa_if.h pccarddevs.h

.include <bsd.kmod.mk>
