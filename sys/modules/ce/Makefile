# $Cronyx: sys.modules.ce.Makefile,v 1.2 2005/04/23 20:10:22 rik Exp $
# $FreeBSD$

.PATH:  ${.CURDIR}/../../dev/ce
KMOD	= if_ce
SRCS	= if_ce.c ceddk.c tau32-ddk.c
SRCS   += opt_netgraph.h opt_ng_cronyx.h device_if.h bus_if.h opt_bpf.h\
	  pci_if.h sppp.h
NOMAN=

NBPF?=		0
PROTOS?=	-DINET
NG_CRONYX?=	0
NETGRAPH=	${NG_CRONYX}

CFLAGS+= ${PROTOS}
CLEANFILES+= opt_ng_cronyx.h opt_netgraph.h pci.h bpf.h sppp.h

opt_netgraph.h:
	echo "#define NETGRAPH ${NETGRAPH}" > opt_netgraph.h

opt_ng_cronyx.h:
.if ${NG_CRONYX} != 0
	echo "#define NETGRAPH_CRONYX 1" > opt_ng_cronyx.h
.else
	echo "" > opt_ng_cronyx.h
.endif

sppp.h:
.if ${NG_CRONYX} == 0
	echo "#define NSPPP 1" > sppp.h
.else
	echo "#define NSPPP 0" > sppp.h
.endif	

opt_bpf.h:
	echo "#define DEV_BPF ${NBPF}" > opt_bpf.h

.include <bsd.kmod.mk>
