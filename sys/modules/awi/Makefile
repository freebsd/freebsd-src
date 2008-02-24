# $FreeBSD: src/sys/modules/awi/Makefile,v 1.4 2005/10/14 23:30:07 yar Exp $

.PATH: ${.CURDIR}/../../dev/awi

KMOD=	if_awi
SRCS=	am79c930.c awi.c if_awi_pccard.c
SRCS+=	bus_if.h card_if.h device_if.h opt_inet.h pccarddevs.h

.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET 1" > opt_inet.h
.endif

.include <bsd.kmod.mk>
