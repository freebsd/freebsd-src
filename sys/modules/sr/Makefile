# $FreeBSD: src/sys/modules/sr/Makefile,v 1.2 2001/02/04 08:23:13 peter Exp $
 
.PATH:  ${.CURDIR}/../../dev/sr
KMOD    = if_sr
SRCS    = if_sr.c if_sr_isa.c if_sr_pci.c
SRCS    += device_if.h bus_if.h pci_if.h isa_if.h opt_netgraph.h
  
.include <bsd.kmod.mk>
