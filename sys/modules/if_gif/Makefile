# $FreeBSD$

.PATH: ${.CURDIR}/../../net ${.CURDIR}/../../netinet ${.CURDIR}/../../netinet6

KMOD=	if_gif
SRCS=	if_gif.c in_gif.c opt_inet.h opt_inet6.h opt_mac.h \
	opt_mrouting.h

.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

.if !defined(NO_INET6)
opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.endif

opt_mrouting.h:
	echo "#define MROUTING 1" > ${.TARGET}
.else
OPT_INET6!= cat ${KERNBUILDDIR}/opt_inet6.h
.if empty(OPT_INET6)
NO_INET6=
.endif
.endif

.if !defined(NO_INET6)
SRCS+=	in6_gif.c
.endif

.include <bsd.kmod.mk>
