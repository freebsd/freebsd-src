# $FreeBSD$

.PATH:  ${.CURDIR}/../../dev/pcic ${.CURDIR}/../../dev/pccard
KMOD=   pcic
SRCS=   i82365.c i82365_isa.c device_if.h bus_if.h isa_if.h \
	pccard.c pccard_cis.c pccard_cis_quirks.c \
	power_if.h card_if.h power_if.c card_if.c
NOMAN=

.include <bsd.kmod.mk>
