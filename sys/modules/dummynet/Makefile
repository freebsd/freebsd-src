# $FreeBSD: src/sys/modules/dummynet/Makefile,v 1.6.8.1 2009/04/15 03:14:26 kensmith Exp $

.include <bsd.own.mk>

.PATH:  ${.CURDIR}/../../netinet
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
