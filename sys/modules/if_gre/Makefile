# $FreeBSD$

SYSDIR?=${.CURDIR}/../..
.PATH: ${SYSDIR}/net ${SYSDIR}/netinet ${SYSDIR}/netinet6
.include "${SYSDIR}/conf/kern.opts.mk"

KMOD=	if_gre
SRCS=	if_gre.c opt_inet.h opt_inet6.h

.if ${MK_INET_SUPPORT} != "no"
SRCS+=	ip_gre.c
.endif

.if ${MK_INET6_SUPPORT} != "no"
SRCS+=	ip6_gre.c
.endif

.include <bsd.kmod.mk>
