# $FreeBSD$

.include <src.opts.mk>

.PATH: ${.CURDIR}/../../net ${.CURDIR}/../../netinet ${.CURDIR}/../../netinet6

KMOD=	if_gif
SRCS=	if_gif.c in_gif.c opt_inet.h opt_inet6.h opt_mrouting.h

.if defined(KERNBUILDDIR)
OPT_INET6!= cat ${KERNBUILDDIR}/opt_inet6.h
.if empty(OPT_INET6)
MK_INET6_SUPPORT=no
.endif
.endif

.if ${MK_INET6_SUPPORT} != "no"
SRCS+=	in6_gif.c
.endif

.include <bsd.kmod.mk>
