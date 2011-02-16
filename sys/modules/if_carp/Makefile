# $FreeBSD: src/sys/modules/if_carp/Makefile,v 1.1.2.2.2.1 2010/12/21 17:09:25 kensmith Exp $

.PATH: ${.CURDIR}/../../netinet

.include <bsd.own.mk>

KMOD=	if_carp
SRCS=	ip_carp.c
SRCS+=	opt_carp.h opt_bpf.h opt_inet.h opt_inet6.h vnode_if.h

.if !defined(KERNBUILDDIR)
opt_inet.h:
	@echo "#define INET 1" > ${.TARGET}

.if ${MK_INET6_SUPPORT} != "no"
opt_inet6.h:
	@echo "#define INET6 1" > ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>
