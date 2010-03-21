# $FreeBSD: src/sys/modules/netgraph/fec/Makefile,v 1.4.32.1 2010/02/10 00:26:20 kensmith Exp $

KMOD=		ng_fec
SRCS= 		ng_fec.c opt_inet.h opt_inet6.h

CFLAGS +=	-DINET -DINET6

.include <bsd.kmod.mk>
