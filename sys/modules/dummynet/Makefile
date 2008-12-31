# $FreeBSD: src/sys/modules/dummynet/Makefile,v 1.6.6.1 2008/11/25 02:59:29 kensmith Exp $

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
