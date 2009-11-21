# $FreeBSD: src/sys/modules/dummynet/Makefile,v 1.7.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

.include <bsd.own.mk>

.PATH:  ${.CURDIR}/../../netinet/ipfw
KMOD=   dummynet
SRCS=   ip_dummynet.c
SRCS+=	opt_inet6.h

.if !defined(KERNBUILDDIR)
.if ${MK_INET6_SUPPORT} != "no"
opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>
