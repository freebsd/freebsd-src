# $FreeBSD: src/sys/modules/joy/Makefile,v 1.22 2001/12/05 09:09:48 imp Exp $

.PATH: ${.CURDIR}/../../dev/joy

KMOD=	joy
SRCS=	bus_if.h device_if.h isa_if.h card_if.h joy.c joy_isa.c joy_pccard.c

.include <bsd.kmod.mk>
