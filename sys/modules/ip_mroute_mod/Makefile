# $FreeBSD: src/sys/modules/ip_mroute_mod/Makefile,v 1.16 2007/06/13 02:08:03 sepotvin Exp $

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../netinet ${.CURDIR}/../../netinet6

KMOD=	ip_mroute

SRCS=	ip_mroute.c
SRCS+=	opt_inet.h opt_mac.h opt_mrouting.h
SRCS+=	opt_inet6.h

.if ${MK_INET6_SUPPORT} != "no"
SRCS+=	ip6_mroute.c
.endif

.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}
opt_mrouting.h:
	echo "#define MROUTING 1" > ${.TARGET}
.if ${MK_INET6_SUPPORT} != "no"
opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>
