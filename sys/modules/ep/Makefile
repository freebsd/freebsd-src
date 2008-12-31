# $FreeBSD: src/sys/modules/ep/Makefile,v 1.9.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/ep

KMOD=	if_ep
SRCS=	if_ep.c
SRCS+=	if_ep_pccard.c pccarddevs.h card_if.h
SRCS+=	if_ep_isa.c isa_if.h
SRCS+=	if_ep_eisa.c eisa_if.h
#SRCS+=	if_ep_mca.c
SRCS+=	bus_if.h device_if.h

.include <bsd.kmod.mk>
