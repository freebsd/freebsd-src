# $Cronyx: sys.modules.cp.Makefile,v 1.1.2.3 2004/04/28 16:17:57 rik Exp $
# $FreeBSD: src/sys/modules/cp/Makefile,v 1.5 2006/03/16 15:18:17 ru Exp $

.PATH:  ${.CURDIR}/../../dev/cp
KMOD	= if_cp
SRCS	= if_cp.c cpddk.c
SRCS   += opt_netgraph.h opt_ng_cronyx.h device_if.h bus_if.h \
	  pci_if.h

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
