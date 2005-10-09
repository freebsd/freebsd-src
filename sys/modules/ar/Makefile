# $FreeBSD$

.PATH:  ${.CURDIR}/../../dev/ar
KMOD    = if_ar
SRCS    = if_ar.c if_ar_isa.c if_ar_pci.c
SRCS    += device_if.h bus_if.h pci_if.h isa_if.h opt_netgraph.h

NETGRAPH?=	0

opt_netgraph.h:
.if ${NETGRAPH} != 0
	echo "#define NETGRAPH ${NETGRAPH}" > opt_netgraph.h
.else
	echo "" > opt_netgraph.h
.endif

.include <bsd.kmod.mk>
