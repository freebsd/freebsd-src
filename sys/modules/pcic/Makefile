# $FreeBSD: src/sys/modules/pcic/Makefile,v 1.13 1999/12/19 05:49:56 imp Exp $

.PATH:  ${.CURDIR}/../../dev/pcic
KMOD=   pcic
SRCS=   i82365.c i82365_isa.c \
	device_if.h bus_if.h isa_if.h power_if.h card_if.h
NOMAN=

.include <bsd.kmod.mk>
