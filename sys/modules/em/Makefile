#$FreeBSD: src/sys/modules/em/Makefile,v 1.3 2002/10/27 17:06:03 mux Exp $
.PATH:  ${.CURDIR}/../../dev/em
KMOD    = if_em
SRCS    = device_if.h bus_if.h pci_if.h opt_bdg.h
SRCS    += if_em.c if_em_hw.c

.include <bsd.kmod.mk>
