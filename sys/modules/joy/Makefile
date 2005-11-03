# $FreeBSD: src/sys/modules/joy/Makefile,v 1.23 2004/05/26 00:53:04 imp Exp $

.PATH: ${.CURDIR}/../../dev/joy

KMOD=	joy
SRCS=	bus_if.h device_if.h isa_if.h card_if.h joy.c joy_isa.c joy_pccard.c \
	pccarddevs.h

.include <bsd.kmod.mk>
