# $Whistle: Makefile,v 1.1 1999/12/08 20:20:39 archie Exp $
# $FreeBSD$

# Determine which of compression and encryption to support
#
NETGRAPH_MPPC_COMPRESSION?=	0
.if !defined(NETGRAPH_MPPC_ENCRYPTION)
.if defined(NOCRYPT)
NETGRAPH_MPPC_ENCRYPTION=	0
.else
NETGRAPH_MPPC_ENCRYPTION=	1
.endif
.endif

# No point in building module if neither is enabled
#
.if ${NETGRAPH_MPPC_COMPRESSION} == 0 && ${NETGRAPH_MPPC_ENCRYPTION} == 0
all depend distribute install load unload:
	@echo '>>>' Neither compression nor encryption enabled, skipping KLD netgraph/mppc.
.else

KMOD=		ng_mppc
SRCS= 		ng_mppc.c opt_netgraph.h
MAN8=		ng_mppc.8
KMODDEPS=	netgraph

CFLAGS+=	${PROTOS}

CLEANFILES+=	opt_netgraph.h

.if ${NETGRAPH_MPPC_COMPRESSION} > 0
# XXX These files don't exist yet, but hopefully someday they will...
SRCS+=		mppcc.c mppcd.c
.PATH:		${.CURDIR}/../../../net
.endif

.if ${NETGRAPH_MPPC_ENCRYPTION} > 0
SRCS+=		rc4.c sha1.c
.PATH:		${.CURDIR}/../../../crypto
.PATH:		${.CURDIR}/../../../crypto/rc4
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
.endif
