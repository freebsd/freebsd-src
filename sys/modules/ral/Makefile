# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/ral

KMOD    = if_ral
SRCS    = if_ral.c if_ralrate.c if_ral_pccard.c if_ral_pci.c \
          device_if.h bus_if.h pci_if.h card_if.h pccarddevs.h

.include <bsd.kmod.mk>
