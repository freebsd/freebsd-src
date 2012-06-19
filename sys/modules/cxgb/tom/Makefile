# $FreeBSD$

.include <bsd.own.mk>

CXGB = ${.CURDIR}/../../../dev/cxgb
.PATH: ${CXGB}/ulp/tom

KMOD=	t3_tom
SRCS=   cxgb_tom.c cxgb_cpl_io.c cxgb_listen.c cxgb_l2t.c
SRCS+=	opt_compat.h opt_inet.h opt_inet6.h opt_ipsec.h
SRCS+=	opt_tcpdebug.h opt_ddb.h opt_sched.h opt_global.h opt_ktr.h
SRCS+=	device_if.h bus_if.h pci_if.h
CFLAGS+= -g -I${CXGB}

#CFLAGS+= -DDEBUG_PRINT -DDEBUG

.if !defined(KERNBUILDDIR)
.if ${MK_INET_SUPPORT} != "no"
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}
	echo "#define TCP_OFFLOAD 1" >> ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>
