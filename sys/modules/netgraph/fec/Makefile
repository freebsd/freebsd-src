# $FreeBSD: src/sys/modules/netgraph/fec/Makefile,v 1.4 2004/01/13 11:30:37 ru Exp $

KMOD=		ng_fec
SRCS= 		ng_fec.c opt_inet.h opt_inet6.h

CFLAGS +=	-DINET -DINET6

.include <bsd.kmod.mk>
