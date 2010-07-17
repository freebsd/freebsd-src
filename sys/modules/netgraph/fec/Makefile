# $FreeBSD: src/sys/modules/netgraph/fec/Makefile,v 1.4.30.1.4.1 2010/06/14 02:09:06 kensmith Exp $

KMOD=		ng_fec
SRCS= 		ng_fec.c opt_inet.h opt_inet6.h

CFLAGS +=	-DINET -DINET6

.include <bsd.kmod.mk>
