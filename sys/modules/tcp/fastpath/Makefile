#
# $FreeBSD$
#

.PATH: ${.CURDIR}/../../../netinet/tcp_stacks

KMOD=	fastpath
SRCS=	fastpath.c

SRCS+=	opt_ipfw.h opt_inet.h opt_inet6.h opt_ipsec.h
SRCS+=	opt_tcpdebug.h

#
# Enable full debugging
#
#CFLAGS += -g

.include <bsd.kmod.mk>
