# $FreeBSD$

.PATH: ${.CURDIR}/../../contrib/pf/net
.PATH: ${.CURDIR}/../../contrib/pf/netinet
.PATH: ${.CURDIR}/../../netinet

KMOD=	pf
SRCS = 	pf.c pf_if.c pf_subr.c pf_osfp.c pf_ioctl.c pf_norm.c pf_table.c \
	if_pflog.c \
	in4_cksum.c \
	opt_pf.h opt_inet.h opt_inet6.h opt_bpf.h

CFLAGS+=  -I${.CURDIR}/../../contrib/pf

.if !defined(KERNBUILDDIR)
opt_pf.h:
	echo "#define DEV_PF 1" > opt_pf.h
	echo "#define DEV_PFLOG 1" >> opt_pf.h

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

.if !defined(NO_INET6)
opt_inet6.h:
	echo "#define INET6 1" > opt_inet6.h
.endif

opt_bpf.h:
	echo "#define DEV_BPF 1" > opt_bpf.h
.endif

.include <bsd.kmod.mk>
