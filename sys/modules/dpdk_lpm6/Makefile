SYSDIR?=${SRCTOP}/sys
.include "${SYSDIR}/conf/kern.opts.mk"

.PATH: ${SYSDIR}/contrib/dpdk_rte_lpm

KMOD=	dpdk_lpm6
SRCS=	opt_inet6.h
SRCS.INET6=dpdk_lpm6.c rte_lpm6.c

.include <bsd.kmod.mk>
