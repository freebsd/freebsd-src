# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/ep

KMOD=	if_ep
SRCS=	if_ep.c if_ep_pccard.c pccarddevs.h
SRCS+=	if_ep_isa.c
#SRCS+=	if_ep_eisa.c
#SRCS+=	if_ep_mca.c
SRCS+=	opt_bdg.h bus_if.h card_if.h device_if.h isa_if.h pci_if.h pccarddevs.h

.include <bsd.kmod.mk>
