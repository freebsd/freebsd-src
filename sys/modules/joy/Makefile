# $FreeBSD: src/sys/modules/joy/Makefile,v 1.23.30.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/joy

KMOD=	joy
SRCS=	bus_if.h device_if.h isa_if.h card_if.h joy.c joy_isa.c joy_pccard.c \
	pccarddevs.h

.include <bsd.kmod.mk>
