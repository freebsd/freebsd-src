# $FreeBSD$

.PATH:	${SRCTOP}/sys/netlink/

KMOD=	nlsysevent
SRCS=	netlink_sysevent.c

CFLAGS+=	-I${SRCTOP}/sys/contrib/netlink
EXPORT_SYMS=	yes

.include <bsd.kmod.mk>
