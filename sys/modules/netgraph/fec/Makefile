# $FreeBSD: src/sys/modules/netgraph/fec/Makefile,v 1.4.24.1 2008/10/02 02:57:24 kensmith Exp $

KMOD=		ng_fec
SRCS= 		ng_fec.c opt_inet.h opt_inet6.h

CFLAGS +=	-DINET -DINET6

.include <bsd.kmod.mk>
