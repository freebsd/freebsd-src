# $FreeBSD$
.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../net ${.CURDIR}/../../netinet ${.CURDIR}/../../netinet6

KMOD=	if_gre
SRCS=	if_gre.c opt_inet.h opt_inet6.h

.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

.if ${MK_INET6_SUPPORT} != "no"
opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.endif
.else
OPT_INET!= cat ${KERNBUILDDIR}/opt_inet.h; echo
.if empty(OPT_INET)
MK_INET_SUPPORT=no
.endif
OPT_INET6!= cat ${KERNBUILDDIR}/opt_inet6.h; echo
.if empty(OPT_INET6)
MK_INET6_SUPPORT=no
.endif
.endif

.if ${MK_INET_SUPPORT} != "no"
SRCS+=  ip_gre.c
.endif

.if ${MK_INET6_SUPPORT} != "no"
SRCS+=  ip6_gre.c
.endif

.include <bsd.kmod.mk>
