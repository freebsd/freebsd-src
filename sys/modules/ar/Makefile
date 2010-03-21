# $FreeBSD: src/sys/modules/ar/Makefile,v 1.5.10.1 2010/02/10 00:26:20 kensmith Exp $

.PATH:  ${.CURDIR}/../../dev/ar
KMOD    = if_ar
SRCS    = if_ar.c if_ar_isa.c if_ar_pci.c
SRCS    += device_if.h bus_if.h pci_if.h isa_if.h opt_netgraph.h

.if !defined(KERNBUILDDIR)
NETGRAPH?=	0

.if ${NETGRAPH} != 0
opt_netgraph.h:
	echo "#define NETGRAPH ${NETGRAPH}" > opt_netgraph.h
.endif
.endif

.include <bsd.kmod.mk>
