# $FreeBSD: src/sys/modules/netgraph/fec/Makefile,v 1.2 2002/12/23 22:33:08 julian Exp $

KMOD=		ng_fec
SRCS= 		ng_fec.c opt_inet.h opt_inet6.h
#MAN4=		ng_fec.4
NOMAN=		yes
MODULE_DEPEND=	netgraph

CFLAGS +=	-DINET -DINET6

.include <bsd.kmod.mk>
