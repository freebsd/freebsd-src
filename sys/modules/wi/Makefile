# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/wi

KMOD=	if_wi
SRCS=	if_wi.c card_if.h device_if.h bus_if.h pci_if.h pci.h

CLEANFILES +=	pci.h

pci.h:
	echo "#define NPCI 1" > $@

.include <bsd.kmod.mk>
