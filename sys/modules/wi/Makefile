# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/wi

KMOD=	if_wi
SRCS=	opt_wi.h if_wi.c if_wi_pccard.c if_wi_pci.c \
	card_if.h device_if.h bus_if.h pci_if.h pccarddevs.h

opt_wi.h:
	echo "#define WI_SYMBOL_FIRMWARE 1" > ${.TARGET}

.include <bsd.kmod.mk>
