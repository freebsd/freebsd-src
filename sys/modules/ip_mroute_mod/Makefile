#	$FreeBSD$

.PATH:	${.CURDIR}/../../sys/netinet
KMOD=	ip_mroute_mod
SRCS=	ip_mroute.c
NOMAN=
CFLAGS+= -DMROUTE_LKM -DMROUTING

.include <bsd.kmod.mk>
