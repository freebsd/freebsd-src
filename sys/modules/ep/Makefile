# $FreeBSD: src/sys/modules/ep/Makefile,v 1.5 2003/08/02 18:46:02 ru Exp $

.PATH: ${.CURDIR}/../../dev/ep

KMOD=	if_ep
SRCS=	if_ep.c if_ep_pccard.c
SRCS+=	if_ep_isa.c
#SRCS+=	if_ep_eisa.c
#SRCS+=	if_ep_mca.c
SRCS+=	opt_bdg.h bus_if.h card_if.h device_if.h isa_if.h pci_if.h

.include <bsd.kmod.mk>
