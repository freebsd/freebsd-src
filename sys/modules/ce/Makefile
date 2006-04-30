# $Cronyx: sys.modules.ce.Makefile,v 1.2 2005/04/23 20:10:22 rik Exp $
# $FreeBSD$

.PATH:  ${.CURDIR}/../../dev/ce
KMOD	= if_ce
SRCS	= if_ce.c ceddk.c tau32-ddk.c
SRCS   += opt_bpf.h opt_netgraph.h opt_ng_cronyx.h device_if.h bus_if.h \
	  pci_if.h
NO_MAN=

.if !defined(KERNBUILDDIR)
PROTOS?=	-DINET
NG_CRONYX?=	0
NETGRAPH=	${NG_CRONYX}

CFLAGS+= ${PROTOS}

.if ${NETGRAPH} != 0
opt_netgraph.h:
	echo "#define NETGRAPH ${NETGRAPH}" > opt_netgraph.h
.endif

.if ${NG_CRONYX} != 0
opt_ng_cronyx.h:
	echo "#define NETGRAPH_CRONYX 1" > opt_ng_cronyx.h
.endif
.endif

.include <bsd.kmod.mk>
