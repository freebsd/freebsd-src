# $FreeBSD: src/sys/modules/pccard/Makefile,v 1.10 2002/01/11 20:14:02 imp Exp $

.PATH: ${.CURDIR}/../../dev/pccard

KMOD=	pccard
SRCS=	pccard.c pccard_cis.c pccard_cis_quirks.c \
	device_if.h bus_if.h card_if.h power_if.h

.include <bsd.kmod.mk>
