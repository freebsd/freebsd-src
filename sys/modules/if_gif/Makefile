# $FreeBSD$

SYSDIR?=${.CURDIR}/../..
.include "${SYSDIR}/conf/kern.opts.mk"

.PATH: ${SYSDIR}/net ${SYSDIR}/netinet ${SYSDIR}/netinet6

KMOD=	if_gif
SRCS=	if_gif.c in_gif.c opt_inet.h opt_inet6.h

.if ${MK_INET6_SUPPORT} != "no"
SRCS+=	in6_gif.c
.endif

.include <bsd.kmod.mk>
