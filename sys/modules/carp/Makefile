# $FreeBSD$

.PATH: ${.CURDIR}/../../netinet
.PATH: ${.CURDIR}/../../crypto

.include <bsd.own.mk>

KMOD=	carp
SRCS=	ip_carp.c sha1.c
SRCS+=	device_if.h bus_if.h vnode_if.h
SRCS+=	opt_carp.h opt_bpf.h opt_inet.h opt_inet6.h opt_ofed.h

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
