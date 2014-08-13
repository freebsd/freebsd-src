# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/snc

KMOD=	if_snc
SRCS=	if_snc.c if_snc_cbus.c if_snc_pccard.c dp83932.c dp83932subr.c
SRCS+=	opt_inet.h device_if.h bus_if.h isa_if.h card_if.h pccarddevs.h

.include <bsd.kmod.mk>
