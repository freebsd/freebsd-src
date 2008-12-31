# Cronyx Id: sys.modules.ct.Makefile,v 1.1.2.2 2003/12/08 15:39:01 rik Exp $
# $FreeBSD: src/sys/modules/ctau/Makefile,v 1.4.6.1 2008/11/25 02:59:29 kensmith Exp $
.PATH:  ${.CURDIR}/../../dev/ctau
KMOD=   if_ct
SRCS=   if_ct.c ctddk.c ctau.c opt_netgraph.h opt_ng_cronyx.h \
	device_if.h bus_if.h isa_if.h

.if !defined(KERNBUILDDIR)
NCTAU?=		3
PROTOS?=	-DINET
NG_CRONYX?=	0
NETGRAPH=	${NG_CRONYX}

CFLAGS+= ${PROTOS}

.if ${NETGRAPH} != 0
opt_netgraph.h:
	echo "#define NETGRAPH $(NETGRAPH)" > opt_netgraph.h
.endif

.if ${NG_CRONYX} != 0
opt_ng_cronyx.h:
	echo "#define NETGRAPH_CRONYX 1" > opt_ng_cronyx.h
.endif
.endif

.include <bsd.kmod.mk>
