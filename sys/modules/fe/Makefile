# $FreeBSD: src/sys/modules/fe/Makefile,v 1.2 2002/11/06 13:46:58 nyan Exp $

.PATH: ${.CURDIR}/../../dev/fe

KMOD=	if_fe
SRCS=	if_fe.c if_fe_pccard.c

.if ${MACHINE} == "pc98"
SRCS+=	if_fe_cbus.c
.else
SRCS+=	if_fe_isa.c
.endif

SRCS+=	bus_if.h card_if.h device_if.h isa_if.h

.include <bsd.kmod.mk>
