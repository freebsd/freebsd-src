# $FreeBSD: src/sys/modules/ip_mroute_mod/Makefile,v 1.12 2004/08/14 15:32:19 dwmalone Exp $

.PATH: ${.CURDIR}/../../netinet

KMOD=	ip_mroute
SRCS=	ip_mroute.c opt_mac.h opt_mrouting.h

CFLAGS+= -DMROUTE_KLD

opt_mrouting.h:
	echo "#define	MROUTING 1" > ${.TARGET}

.include <bsd.kmod.mk>
