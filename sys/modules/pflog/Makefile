# $FreeBSD: src/sys/modules/pflog/Makefile,v 1.6.6.1 2008/11/25 02:59:29 kensmith Exp $

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../contrib/pf/net

KMOD=	pflog
SRCS = 	if_pflog.c \
	opt_pf.h opt_inet.h opt_inet6.h opt_bpf.h

CFLAGS+=  -I${.CURDIR}/../../contrib/pf

.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET 1" > opt_inet.h

.if ${MK_INET6_SUPPORT} != "no"
opt_inet6.h:
	echo "#define INET6 1" > opt_inet6.h
.endif

opt_bpf.h:
	echo "#define DEV_BPF 1" > opt_bpf.h
.endif

.include <bsd.kmod.mk>
