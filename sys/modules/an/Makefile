# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/an

KMOD=	if_an
SRCS=	if_an.c if_an_pccard.c if_an_pci.c if_an_isa.c
SRCS+=	opt_inet.h device_if.h bus_if.h pci_if.h isa_if.h card_if.h

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

.include <bsd.kmod.mk>
