# $FreeBSD$

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../net ${.CURDIR}/../../netinet ${.CURDIR}/../../netinet6

KMOD=	if_gif
SRCS=	if_gif.c opt_inet.h opt_inet6.h opt_mrouting.h

.if !defined(KERNBUILDDIR)
.if ${MK_INET_SUPPORT} != "no"
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}
.endif
.else
OPT_INET!= cat ${KERNBUILDDIR}/opt_inet.h; echo
.if empty(OPT_INET)
MK_INET_SUPPORT= no
.endif
.endif

.if !defined(KERNBUILDDIR)
.if ${MK_INET6_SUPPORT} != "no"
opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.endif

opt_mrouting.h:
	echo "#define MROUTING 1" > ${.TARGET}
.else
OPT_INET6!= cat ${KERNBUILDDIR}/opt_inet6.h; echo
.if empty(OPT_INET6)
MK_INET6_SUPPORT= no
.endif
.endif

.if ${MK_INET_SUPPORT} != "no"
SRCS+=	in_gif.c
.endif

.if ${MK_INET6_SUPPORT} != "no"
SRCS+=	in6_gif.c
.endif

.include <bsd.kmod.mk>
