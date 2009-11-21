# $FreeBSD: src/sys/modules/netgraph/fec/Makefile,v 1.4.30.1.2.1 2009/10/25 01:10:29 kensmith Exp $

KMOD=		ng_fec
SRCS= 		ng_fec.c opt_inet.h opt_inet6.h

CFLAGS +=	-DINET -DINET6

.include <bsd.kmod.mk>
