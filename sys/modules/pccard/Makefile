# $FreeBSD: src/sys/modules/pccard/Makefile,v 1.3 1999/11/28 21:11:09 imp Exp $

.PATH:  ${.CURDIR}/../../dev/pccard
KMOD=   pccard
SRCS=   pccard.c pccard_cis.c pccard_cis_quirks.c \
	device_if.h bus_if.h card_if.h card_if.c power_if.h power_if.c
NOMAN=

.include <bsd.kmod.mk>
