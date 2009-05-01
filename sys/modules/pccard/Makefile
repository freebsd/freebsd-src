# $FreeBSD: src/sys/modules/pccard/Makefile,v 1.12.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/pccard

KMOD=	pccard
SRCS=	pccard.c pccard_cis.c pccard_cis_quirks.c pccard_device.c \
	device_if.h bus_if.h card_if.h power_if.h pccarddevs.h

.include <bsd.kmod.mk>
