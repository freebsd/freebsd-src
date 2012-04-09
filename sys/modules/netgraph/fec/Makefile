# $FreeBSD: src/sys/modules/netgraph/fec/Makefile,v 1.4.30.1.8.1 2012/03/03 06:15:13 kensmith Exp $

KMOD=		ng_fec
SRCS= 		ng_fec.c opt_inet.h opt_inet6.h

CFLAGS +=	-DINET -DINET6

.include <bsd.kmod.mk>
