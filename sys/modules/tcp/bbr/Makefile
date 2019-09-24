#
# $FreeBSD$
#

.PATH: ${.CURDIR}/../../../netinet/tcp_stacks

STACKNAME=	bbr
KMOD=	tcp_${STACKNAME}
SRCS=	bbr.c sack_filter.c rack_bbr_common.c

SRCS+=	opt_inet.h opt_inet6.h opt_ipsec.h
SRCS+=	opt_tcpdebug.h
SRCS+=	opt_kern_tls.h

#
# Enable full debugging
#
#CFLAGS += -g

CFLAGS+=	-DMODNAME=${KMOD}
CFLAGS+=	-DSTACKNAME=${STACKNAME}

.include <bsd.kmod.mk>
