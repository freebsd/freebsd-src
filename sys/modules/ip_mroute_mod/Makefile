# $FreeBSD$

.PATH:	${.CURDIR}/../../netinet
KMOD=	ip_mroute
SRCS=	ip_mroute.c
NOMAN=
CFLAGS+= -DMROUTE_LKM -DMROUTING

.include <bsd.kmod.mk>
