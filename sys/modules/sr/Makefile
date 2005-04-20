# $FreeBSD$
 
.PATH:  ${.CURDIR}/../../dev/sr
KMOD    = if_sr
SRCS    = if_sr.c if_sr_isa.c if_sr_pci.c
SRCS    += device_if.h bus_if.h pci_if.h isa_if.h opt_netgraph.h

NETGRAPH?=    0

opt_netgraph.h:
.if ${NETGRAPH} != 0
	echo "#define NETGRAPH ${NETGRAPH}" > opt_netgraph.h
.else
	echo "" > opt_netgraph.h
.endif

.include <bsd.kmod.mk>
