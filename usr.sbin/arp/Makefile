.include <src.opts.mk>

PROG=	arp
MAN=	arp.4 arp.8


SRCS=	arp.c

.if ${MK_NETLINK_SUPPORT} != "no"
SRCS+=	arp_netlink.c
.else
CFLAGS+=-DWITHOUT_NETLINK
.endif

LIBADD=	xo

WARNS?=	3

.include <bsd.prog.mk>
