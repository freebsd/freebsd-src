# $FreeBSD: src/sys/modules/netgraph/fec/Makefile,v 1.1.4.1 2002/12/27 20:19:42 julian Exp $

KMOD=		ng_fec
SRCS= 		ng_fec.c opt_inet.h opt_inet6.h
#MAN4=		ng_fec.4
NOMAN=		yes
MODULE_DEPEND=	netgraph

CFLAGS +=	-DINET -DINET6

.include <bsd.kmod.mk>
