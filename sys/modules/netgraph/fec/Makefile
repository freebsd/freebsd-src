# $FreeBSD$

KMOD=		ng_fec
SRCS= 		ng_fec.c opt_inet.h opt_inet6.h
#MAN4=		ng_fec.4
NOMAN=		yes
KMODDEPS=	netgraph

CFLAGS +=	-DINET -DINET6

.include <bsd.kmod.mk>
