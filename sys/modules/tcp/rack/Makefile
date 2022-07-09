#
# $FreeBSD$
#

.PATH: ${.CURDIR}/../../../netinet/tcp_stacks

STACKNAME=	rack
KMOD=	tcp_${STACKNAME}
SRCS=	rack.c sack_filter.c rack_bbr_common.c

SRCS+=	opt_inet.h opt_inet6.h opt_ipsec.h
SRCS+=	opt_kern_tls.h
SRCS+=	opt_ratelimit.h
SRCS+=	opt_tcpdebug.h

#
# Enable full debugging
#
#CFLAGS += -g

CFLAGS+=	-DMODNAME=${KMOD}
CFLAGS+=	-DSTACKNAME=${STACKNAME}

.include <bsd.kmod.mk>
