# $FreeBSD$

.PATH: ${.CURDIR}/../../netinet

KMOD=	ip_mroute
SRCS=	ip_mroute.c opt_mac.h opt_mrouting.h

CFLAGS+= -DMROUTE_KLD

opt_mrouting.h:
	echo "#define	MROUTING 1" > ${.TARGET}

.include <bsd.kmod.mk>
