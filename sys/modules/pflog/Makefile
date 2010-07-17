# $FreeBSD: src/sys/modules/pflog/Makefile,v 1.7.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../contrib/pf/net

KMOD=	pflog
SRCS = 	if_pflog.c \
	opt_pf.h opt_inet.h opt_inet6.h opt_bpf.h

CFLAGS+=  -I${.CURDIR}/../../contrib/pf

.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

.if ${MK_INET6_SUPPORT} != "no"
opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.endif

opt_bpf.h:
	echo "#define DEV_BPF 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
