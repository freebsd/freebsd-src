# $FreeBSD: src/sys/modules/pf/Makefile,v 1.13 2007/07/03 12:46:06 mlaier Exp $

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../contrib/pf/net
.PATH: ${.CURDIR}/../../contrib/pf/netinet

KMOD=	pf
SRCS = 	pf.c pf_if.c pf_subr.c pf_osfp.c pf_ioctl.c pf_norm.c pf_table.c \
	pf_ruleset.c \
	in4_cksum.c \
	opt_pf.h opt_inet.h opt_inet6.h opt_bpf.h opt_mac.h

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

# pflog can be loaded as a module, have the additional checks turned on
opt_pf.h:
	echo "#define DEV_PF 1" > opt_pf.h
	echo "#define DEF_PFLOG 1" >> opt_pf.h
.endif

.include <bsd.kmod.mk>
