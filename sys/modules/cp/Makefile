# $Cronyx Id: Makefile,v 1.1.2.4 2004/05/20 16:13:39 rik Exp $
# $FreeBSD$

.PATH:  ${.CURDIR}/../../dev/cp
KMOD=   if_cp
SRCS=   if_cp.c cpddk.c cp.h pci.h opt_netgraph.h opt_ng_cronyx.h device_if.h \
	bus_if.h bpf.h pci_if.h sppp.h
NOMAN=

NBPF?=		0
NPCI=		1
PROTOS?=	-DINET
NG_CRONYX?=	0
NETGRAPH=	${NG_CRONYX}

CFLAGS+= ${PROTOS}
CLEANFILES+= opt_ng_cronyx.h opt_netgraph.h cp.h pci.h bpf.h sppp.h

cp.h:
	echo "" > cp.h

pci.h:
	echo "#define NPCI ${NPCI}" > pci.h

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

bpf.h:
	echo "#define NBPF ${NBPF}" > bpf.h

.include <bsd.kmod.mk>
