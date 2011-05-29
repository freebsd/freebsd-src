# $FreeBSD$

.PATH: ${.CURDIR}/../../netinet

.include <bsd.own.mk>

KMOD=	if_carp
SRCS=	ip_carp.c
SRCS+=	opt_carp.h opt_bpf.h opt_inet.h opt_inet6.h vnode_if.h

.if !defined(KERNBUILDDIR)
.if ${MK_INET_SUPPORT} != "no"
opt_inet.h:
	@echo "#define INET 1" > ${.TARGET}
.endif

.if ${MK_INET6_SUPPORT} != "no"
opt_inet6.h:
	@echo "#define INET6 1" > ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>
