# $FreeBSD: src/sys/modules/ral/Makefile,v 1.3.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/ral

KMOD    = if_ral
SRCS    = rt2560.c rt2661.c if_ralrate.c if_ral_pci.c \
          device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
