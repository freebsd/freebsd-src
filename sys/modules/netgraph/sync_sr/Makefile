# $FreeBSD$

.PATH:  ${.CURDIR}/../../../dev/sr
KMOD    = ng_sync_sr
SRCS    = if_sr.c if_sr_isa.c if_sr_pci.c
SRCS    += device_if.h bus_if.h pci_if.h isa_if.h opt_netgraph.h

.if !defined(KERNBUILDDIR)
opt_netgraph.h:
	echo "#define NETGRAPH" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
