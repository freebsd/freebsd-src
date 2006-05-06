# $FreeBSD: src/sys/modules/ep/Makefile,v 1.8 2005/03/17 18:40:37 imp Exp $

.PATH: ${.CURDIR}/../../dev/ep

KMOD=	if_ep
SRCS=	if_ep.c if_ep_pccard.c pccarddevs.h
SRCS+=	if_ep_isa.c
SRCS+=	if_ep_eisa.c eisa_if.h
#SRCS+=	if_ep_mca.c
SRCS+=	opt_bdg.h bus_if.h card_if.h device_if.h isa_if.h pci_if.h pccarddevs.h

.include <bsd.kmod.mk>
