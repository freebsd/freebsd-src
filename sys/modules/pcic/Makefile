# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/pcic ${.CURDIR}/../../dev/pccard

KMOD=	pcic
SRCS=	i82365.c i82365_isa.c device_if.h bus_if.h isa_if.h \
	power_if.h card_if.h power_if.c card_if.c pccarddevs.h

.include <bsd.kmod.mk>
