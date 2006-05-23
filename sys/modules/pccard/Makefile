# $FreeBSD: src/sys/modules/pccard/Makefile,v 1.11 2004/05/26 00:53:07 imp Exp $

.PATH: ${.CURDIR}/../../dev/pccard

KMOD=	pccard
SRCS=	pccard.c pccard_cis.c pccard_cis_quirks.c \
	device_if.h bus_if.h card_if.h power_if.h pccarddevs.h

.include <bsd.kmod.mk>
