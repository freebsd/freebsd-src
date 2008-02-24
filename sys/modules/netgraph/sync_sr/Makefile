# $FreeBSD: src/sys/modules/netgraph/sync_sr/Makefile,v 1.2 2005/10/14 23:30:13 yar Exp $
 
.PATH:  ${.CURDIR}/../../../dev/sr
KMOD    = ng_sync_sr
SRCS    = if_sr.c if_sr_isa.c if_sr_pci.c
SRCS    += device_if.h bus_if.h pci_if.h isa_if.h opt_netgraph.h
  
.if !defined(KERNBUILDDIR)
opt_netgraph.h:
	echo "#define NETGRAPH" > opt_netgraph.h
.endif

.include <bsd.kmod.mk>
