# $FreeBSD: src/sys/modules/joy/Makefile,v 1.23.26.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/joy

KMOD=	joy
SRCS=	bus_if.h device_if.h isa_if.h card_if.h joy.c joy_isa.c joy_pccard.c \
	pccarddevs.h

.include <bsd.kmod.mk>
