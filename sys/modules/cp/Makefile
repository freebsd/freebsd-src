# $Cronyx: sys.modules.cp.Makefile,v 1.1.2.3 2004/04/28 16:17:57 rik Exp $
# $FreeBSD$

.PATH:  ${.CURDIR}/../../dev/cp
KMOD	= if_cp
SRCS	= if_cp.c cpddk.c
SRCS   += opt_netgraph.h opt_ng_cronyx.h device_if.h bus_if.h bpf.h\
	  pci_if.h sppp.h
NO_MAN=

NBPF?=		0
PROTOS?=	-DINET
NG_CRONYX?=	0
NETGRAPH=	${NG_CRONYX}

CFLAGS+= ${PROTOS}
CLEANFILES+= bpf.h sppp.h

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
