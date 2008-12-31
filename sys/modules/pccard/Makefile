# $FreeBSD: src/sys/modules/pccard/Makefile,v 1.12.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/pccard

KMOD=	pccard
SRCS=	pccard.c pccard_cis.c pccard_cis_quirks.c pccard_device.c \
	device_if.h bus_if.h card_if.h power_if.h pccarddevs.h

.include <bsd.kmod.mk>
