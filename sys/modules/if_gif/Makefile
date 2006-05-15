# $FreeBSD$

.PATH: ${.CURDIR}/../../net ${.CURDIR}/../../netinet ${.CURDIR}/../../netinet6

KMOD=	if_gif
SRCS=	if_gif.c in_gif.c opt_inet.h opt_inet6.h opt_mrouting.h
NOMAN=
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
