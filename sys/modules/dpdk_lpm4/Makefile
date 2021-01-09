# $FreeBSD$

SYSDIR?=${SRCTOP}/sys
.include "${SYSDIR}/conf/kern.opts.mk"

.PATH: ${SYSDIR}/contrib/dpdk_rte_lpm

KMOD=	dpdk_lpm4
SRCS=	opt_inet.h
SRCS.INET=dpdk_lpm.c rte_lpm.c

.include <bsd.kmod.mk>
