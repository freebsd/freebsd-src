# $FreeBSD$

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../contrib/pf/net
.PATH: ${.CURDIR}/../../contrib/pf/netinet

KMOD=	pf
SRCS=	pf.c pf_if.c pf_lb.c pf_osfp.c pf_ioctl.c pf_norm.c pf_table.c \
	pf_ruleset.c \
	in4_cksum.c \
	opt_pf.h opt_inet.h opt_inet6.h opt_bpf.h opt_global.h

CFLAGS+= -I${.CURDIR}/../../contrib/pf

.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

.if ${MK_INET6_SUPPORT} != "no"
opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.endif

opt_bpf.h:
	echo "#define DEV_BPF 1" > ${.TARGET}

# pflog can be loaded as a module, have the additional checks turned on
# pfsync can be loaded as a module, have the additional checks turned on
opt_pf.h:
	echo "#define DEV_PF 1" > ${.TARGET}
	echo "#define DEV_PFLOG 1" >> ${.TARGET}
	echo "#define DEV_PFSYNC 1" >> ${.TARGET}
	echo "#define DEV_PFLOW 1" >> ${.TARGET}

.if defined(VIMAGE)
opt_global.h:
	echo "#define VIMAGE 1" >> ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>
