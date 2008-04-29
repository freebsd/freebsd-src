# $FreeBSD$

.PATH:  ${.CURDIR}/../../dev/iwn

KMOD    = if_iwn
SRCS    = if_iwn.c opt_bdg.h device_if.h bus_if.h pci_if.h
CFLAGS += -g -DWITNESS -DINVARIANT_SUPPORT -DINVARIANTS -I${.CURDIR}/../../ 
.include <bsd.kmod.mk>
