# $FreeBSD$ 

.PATH:	${.CURDIR}/../../netinet
KMOD=	ipl
SRCS=	mlfk_ipl.c ip_nat.c ip_frag.c ip_state.c ip_proxy.c ip_auth.c \
	ip_log.c ip_fil.c fil.c
NOMAN=
CFLAGS+= -DIPFILTER=1 -DIPFILTER_LKM -DIPFILTER_LOG
#
# If you don't want log functionality remove -DIPFILTER_LOG
#

.include <bsd.kmod.mk>
