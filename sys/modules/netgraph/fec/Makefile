# $FreeBSD: src/sys/modules/netgraph/fec/Makefile,v 1.4.28.1 2009/04/15 03:14:26 kensmith Exp $

KMOD=		ng_fec
SRCS= 		ng_fec.c opt_inet.h opt_inet6.h

CFLAGS +=	-DINET -DINET6

.include <bsd.kmod.mk>
