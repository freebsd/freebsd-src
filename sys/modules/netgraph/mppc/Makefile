# $Whistle: Makefile,v 1.1 1999/12/08 20:20:39 archie Exp $
# $FreeBSD: src/sys/modules/netgraph/mppc/Makefile,v 1.3.2.2 2000/05/04 18:43:27 archie Exp $

KMOD=		ng_mppc
SRCS= 		ng_mppc.c opt_netgraph.h
MAN4=		ng_mppc.4
KMODDEPS=	netgraph

NETGRAPH_MPPC_COMPRESSION?=	0
NETGRAPH_MPPC_ENCRYPTION?=	1

.if ${NETGRAPH_MPPC_COMPRESSION} > 0
# XXX These files don't exist yet, but hopefully someday they will...
.PATH:		${.CURDIR}/../../../net
SRCS+=		mppcc.c mppcd.c
.endif

.if ${NETGRAPH_MPPC_ENCRYPTION} > 0
.PATH:		${.CURDIR}/../../../crypto
.PATH:		${.CURDIR}/../../../crypto/rc4
SRCS+=		rc4.c sha1.c
.endif

opt_netgraph.h:
	touch ${.TARGET}
.if ${NETGRAPH_MPPC_COMPRESSION} > 0
	echo "#define NETGRAPH_MPPC_COMPRESSION 1" >> ${.TARGET}
.endif
.if ${NETGRAPH_MPPC_ENCRYPTION} > 0
	echo "#define NETGRAPH_MPPC_ENCRYPTION 1" >> ${.TARGET}
.endif

.include <bsd.kmod.mk>
