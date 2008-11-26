# $FreeBSD: src/sys/modules/joy/Makefile,v 1.23.24.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/joy

KMOD=	joy
SRCS=	bus_if.h device_if.h isa_if.h card_if.h joy.c joy_isa.c joy_pccard.c \
	pccarddevs.h

.include <bsd.kmod.mk>
