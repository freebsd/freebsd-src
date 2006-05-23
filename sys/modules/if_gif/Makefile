# $FreeBSD: src/sys/modules/if_gif/Makefile,v 1.3.8.1 2006/01/29 07:46:26 avatar Exp $

.PATH: ${.CURDIR}/../../net ${.CURDIR}/../../netinet ${.CURDIR}/../../netinet6

KMOD=	if_gif
SRCS=	if_gif.c in_gif.c opt_inet.h opt_inet6.h opt_mac.h \
	opt_mrouting.h
.if !defined(NOINET6)
SRCS+=	in6_gif.c
.endif

opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

.if !defined(NOINET6)
opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.endif

opt_mrouting.h:
	echo "#define MROUTING 1" > ${.TARGET}

.include <bsd.kmod.mk>
