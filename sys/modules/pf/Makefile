# $FreeBSD$

.PATH: ${.CURDIR}/../../contrib/pf/net
.PATH: ${.CURDIR}/../../contrib/pf/netinet

KMOD=	pf
SRCS = 	pf.c pf_osfp.c pf_ioctl.c pf_norm.c pf_table.c \
	in4_cksum.c \
	opt_pf.h opt_inet.h opt_inet6.h opt_bpf.h opt_random_ip_id.h

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

opt_random_ip_id.h:
	echo "#define RANDOM_IP_ID 1" > opt_random_ip_id.h

.include <bsd.kmod.mk>
