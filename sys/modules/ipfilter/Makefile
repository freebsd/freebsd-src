# $FreeBSD: src/sys/modules/ipfilter/Makefile,v 1.12 2003/09/23 17:55:04 sam Exp $ 

.PATH: ${.CURDIR}/../../contrib/ipfilter/netinet

KMOD=	ipl
SRCS=	mlfk_ipl.c ip_nat.c ip_frag.c ip_state.c ip_proxy.c ip_auth.c \
	ip_log.c ip_fil.c fil.c

CFLAGS+=  -I${.CURDIR}/../../contrib/ipfilter
CFLAGS+= -DIPFILTER=1 -DIPFILTER_LKM -DIPFILTER_LOG -DPFIL_HOOKS
#
# If you don't want log functionality remove -DIPFILTER_LOG
#

.include <bsd.kmod.mk>
