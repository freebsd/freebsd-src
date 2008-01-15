# $FreeBSD: src/sys/modules/pccard/Makefile,v 1.11.8.1 2005/09/27 18:42:19 imp Exp $

.PATH: ${.CURDIR}/../../dev/pccard

KMOD=	pccard
SRCS=	pccard.c pccard_cis.c pccard_cis_quirks.c pccard_device.c \
	device_if.h bus_if.h card_if.h power_if.h pccarddevs.h

.include <bsd.kmod.mk>
