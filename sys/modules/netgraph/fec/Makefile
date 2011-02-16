# $FreeBSD: src/sys/modules/netgraph/fec/Makefile,v 1.4.34.1 2010/12/21 17:10:29 kensmith Exp $

KMOD=		ng_fec
SRCS= 		ng_fec.c opt_inet.h opt_inet6.h

CFLAGS +=	-DINET -DINET6

.include <bsd.kmod.mk>
