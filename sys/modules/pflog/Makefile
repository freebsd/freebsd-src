# $FreeBSD$

.PATH: ${.CURDIR}/../../contrib/pf/net

KMOD=	pflog
SRCS = 	if_pflog.c \
	opt_pf.h opt_inet.h opt_inet6.h opt_bpf.h

CFLAGS+=  -Wall -I${.CURDIR}/../../contrib/pf

opt_pf.h:
	echo "#define DEV_PF 1" > opt_pf.h
	echo "#define DEV_PFLOG 1" >> opt_pf.h
	echo "#define DEV_PFSYNC 1" >> opt_pf.h

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

opt_inet6.h:
.if defined(NOINET6)
	echo > opt_inet6.h
.else
	echo "#define INET6 1" > opt_inet6.h
.endif

opt_bpf.h:
	echo "#define DEV_BPF 1" > opt_bpf.h

.include <bsd.kmod.mk>
