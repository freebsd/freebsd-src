# $FreeBSD: src/sys/modules/ip_mroute_mod/Makefile,v 1.4.2.1 1999/08/29 16:27:19 peter Exp $

.PATH:	${.CURDIR}/../../netinet
KMOD=	ip_mroute
SRCS=	ip_mroute.c
NOMAN=
CFLAGS+= -DMROUTE_LKM -DMROUTING

.include <bsd.kmod.mk>
