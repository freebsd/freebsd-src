#	$Id: Makefile,v 1.1 1994/09/14 22:11:51 wollman Exp $

.PATH:	${.CURDIR}/../../sys/netinet
KMOD=	ip_mroute_mod
SRCS=	ip_mroute.c
NOMAN=
CFLAGS+= -DMROUTE_LKM -DMROUTING

.include <bsd.kmod.mk>
