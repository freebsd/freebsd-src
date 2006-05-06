# $FreeBSD: src/sys/modules/ral/Makefile,v 1.1 2005/04/18 18:47:38 damien Exp $

.PATH: ${.CURDIR}/../../dev/ral

KMOD    = if_ral
SRCS    = if_ral.c if_ralrate.c if_ral_pccard.c if_ral_pci.c \
          opt_bdg.h device_if.h bus_if.h pci_if.h card_if.h pccarddevs.h

.include <bsd.kmod.mk>
