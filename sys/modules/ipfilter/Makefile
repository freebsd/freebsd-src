# $FreeBSD$ 

.PATH:	${.CURDIR}/../../netinet
KMOD=	ipl
SRCS=	mlfk_ipl.c ip_nat.c ip_frag.c ip_state.c ip_proxy.c ip_auth.c \
	ip_log.c ip_fil.c fil.c opt_ipfilter.h
NOMAN=
CFLAGS+= -DIPFILTER_LKM -DIPFILTER_LOG
#
# If you don't want long functionality remove -DIPFILTER_LOG
#

opt_ipfilter.h:
	echo "#define IPFILTER 1" > opt_ipfilter.h

.include <bsd.kmod.mk>
