# $FreeBSD: src/sys/modules/sr/Makefile,v 1.4 2005/10/14 23:30:16 yar Exp $
 
.PATH:  ${.CURDIR}/../../dev/sr
KMOD    = if_sr
SRCS    = if_sr.c if_sr_isa.c if_sr_pci.c
SRCS    += device_if.h bus_if.h pci_if.h isa_if.h opt_netgraph.h

.if !defined(KERNBUILDDIR)
NETGRAPH?=    0

.if ${NETGRAPH} != 0
opt_netgraph.h:
	echo "#define NETGRAPH ${NETGRAPH}" > opt_netgraph.h
.endif
.endif

.include <bsd.kmod.mk>
