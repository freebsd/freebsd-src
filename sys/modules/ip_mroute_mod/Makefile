#	$Id: Makefile,v 1.3 1997/02/22 12:48:12 peter Exp $

.PATH:	${.CURDIR}/../../netinet
KMOD=	ip_mroute
SRCS=	ip_mroute.c
NOMAN=
CFLAGS+= -DMROUTE_LKM -DMROUTING

.include <bsd.kmod.mk>
