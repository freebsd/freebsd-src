# $Whistle: Makefile,v 1.1 1999/12/08 20:20:39 archie Exp $
# $FreeBSD: src/sys/modules/netgraph/mppc/Makefile,v 1.14.6.1 2008/11/25 02:59:29 kensmith Exp $

KMOD=	ng_mppc
SRCS= 	ng_mppc.c opt_netgraph.h

NETGRAPH_MPPC_COMPRESSION?=	0
NETGRAPH_MPPC_ENCRYPTION?=	1

.if ${NETGRAPH_MPPC_COMPRESSION} > 0
# XXX These files don't exist yet, but hopefully someday they will...
.PATH: ${.CURDIR}/../../../net
SRCS+=	mppcc.c mppcd.c
.endif

.if ${NETGRAPH_MPPC_ENCRYPTION} > 0
.PATH: ${.CURDIR}/../../../crypto
SRCS+= sha1.c
.endif

.if !defined(KERNBUILDDIR)
opt_netgraph.h:
	:> ${.TARGET}
.if ${NETGRAPH_MPPC_COMPRESSION} > 0
	echo "#define NETGRAPH_MPPC_COMPRESSION 1" >> ${.TARGET}
.endif
.if ${NETGRAPH_MPPC_ENCRYPTION} > 0
	echo "#define NETGRAPH_MPPC_ENCRYPTION 1" >> ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>
