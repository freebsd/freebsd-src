# $FreeBSD: src/sys/modules/ral/Makefile,v 1.1.2.1.4.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/ral

KMOD    = if_ral
SRCS    = rt2560.c rt2661.c if_ralrate.c if_ral_pci.c \
          device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
