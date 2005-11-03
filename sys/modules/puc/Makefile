
# $FreeBSD: src/sys/modules/puc/Makefile,v 1.1 2005/04/22 07:43:27 imp Exp $

.PATH:	${.CURDIR}/../../dev/puc
KMOD=	puc
SRCS=	bus_if.h device_if.h pci_if.h card_if.h \
	puc.c puc_pci.c puc_pccard.c pucdata.c \
	opt_puc.h

.include <bsd.kmod.mk>
