# $Whistle: Makefile,v 1.1 1999/12/08 20:20:39 archie Exp $
# $FreeBSD$

KMOD=		ng_mppc
SRCS= 		ng_mppc.c opt_netgraph.h
MAN8=		ng_mppc.8
KMODDEPS=	netgraph

NETGRAPH_MPPC_COMPRESSION?=	0
NETGRAPH_MPPC_ENCRYPTION?=	1

CFLAGS+=	${PROTOS}

CLEANFILES+=	opt_netgraph.h

.if ${NETGRAPH_MPPC_COMPRESSION} > 0
# XXX These files don't exist yet, but hopefully someday they will...
SRCS+=		mppcc.c mppcd.c
.endif

.if ${NETGRAPH_MPPC_ENCRYPTION} > 0
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

.PATH:		${.CURDIR}/../../../net
.PATH:		${.CURDIR}/../../../crypto
.PATH:		${.CURDIR}/../../../crypto/rc4

.include <bsd.kmod.mk>
