# $FreeBSD: src/sys/modules/ip_mroute_mod/Makefile,v 1.5 1999/08/28 00:47:20 peter Exp $

.PATH:	${.CURDIR}/../../netinet
KMOD=	ip_mroute
SRCS=	ip_mroute.c
NOMAN=
CFLAGS+= -DMROUTE_LKM -DMROUTING

.include <bsd.kmod.mk>
