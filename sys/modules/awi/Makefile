# $FreeBSD: src/sys/modules/awi/Makefile,v 1.3 2004/05/26 00:53:01 imp Exp $

.PATH: ${.CURDIR}/../../dev/awi

KMOD=	if_awi
SRCS=	am79c930.c awi.c if_awi_pccard.c
SRCS+=	bus_if.h card_if.h device_if.h opt_inet.h pccarddevs.h

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

.include <bsd.kmod.mk>
