#	$Id: Makefile,v 1.4 1998/10/16 04:30:46 peter Exp $

.PATH:	${.CURDIR}/../../netinet
KMOD=	ip_mroute
SRCS=	ip_mroute.c
NOMAN=
CFLAGS+= -DMROUTE_LKM -DMROUTING

.include <bsd.kmod.mk>
