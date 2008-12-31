# $FreeBSD: src/sys/modules/fe/Makefile,v 1.3.26.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/fe

KMOD=	if_fe
SRCS=	if_fe.c if_fe_pccard.c

.if ${MACHINE} == "pc98"
SRCS+=	if_fe_cbus.c
.else
SRCS+=	if_fe_isa.c
.endif

SRCS+=	bus_if.h card_if.h device_if.h isa_if.h pccarddevs.h

.include <bsd.kmod.mk>
