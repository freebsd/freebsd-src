# $FreeBSD$

KMOD=		ng_fec
SRCS= 		ng_fec.c opt_inet.h opt_inet6.h
MODULE_DEPEND=	netgraph

CFLAGS +=	-DINET -DINET6

.include <bsd.kmod.mk>
