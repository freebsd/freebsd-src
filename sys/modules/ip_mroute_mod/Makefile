# $FreeBSD$

.PATH: ${.CURDIR}/../../netinet

KMOD=	ip_mroute
SRCS=	ip_mroute.c opt_mac.h opt_mrouting.h opt_random_ip_id.h

CFLAGS+= -DMROUTE_KLD

RANDOM_IP_ID?=	0	# 0/1 - should jibe with kernel configuration

opt_mrouting.h:
	echo "#define	MROUTING 1" > ${.TARGET}

opt_random_ip_id.h:
	touch ${.TARGET}
.if ${RANDOM_IP_ID} > 0
	echo "#define	RANDOM_IP_ID 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
