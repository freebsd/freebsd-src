# $FreeBSD: src/sys/modules/wi/Makefile,v 1.15 2002/08/03 00:21:29 imp Exp $

.PATH: ${.CURDIR}/../../dev/wi

KMOD=	if_wi
SRCS=	opt_wi.h if_wi.c if_wi_pccard.c if_wi_pci.c wi_hostap.c \
	card_if.h device_if.h bus_if.h pci_if.h

opt_wi.h:
	echo "#define WI_SYMBOL_FIRMWARE 1" > ${.TARGET}

.include <bsd.kmod.mk>
