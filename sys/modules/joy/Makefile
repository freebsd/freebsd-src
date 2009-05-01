# $FreeBSD: src/sys/modules/joy/Makefile,v 1.23.28.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/joy

KMOD=	joy
SRCS=	bus_if.h device_if.h isa_if.h card_if.h joy.c joy_isa.c joy_pccard.c \
	pccarddevs.h

.include <bsd.kmod.mk>
