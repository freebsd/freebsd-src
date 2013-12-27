# $FreeBSD$

.include <bsd.own.mk>

CXGB = ${.CURDIR}/../../../dev/cxgb
.PATH: ${CXGB}/ulp/iw_cxgb

KMOD=   iw_cxgb
SRCS=   iw_cxgb.c iw_cxgb_cm.c iw_cxgb_hal.c 
SRCS+=  iw_cxgb_provider.c iw_cxgb_qp.c iw_cxgb_resource.c
SRCS+=  iw_cxgb_ev.c iw_cxgb_mem.c iw_cxgb_dbg.c iw_cxgb_cq.c
SRCS+=  bus_if.h device_if.h opt_sched.h pci_if.h pcib_if.h opt_ktr.h
SRCS+=  opt_inet.h opt_ofed.h vnode_if.h
CFLAGS+= -I${CXGB} -I${.CURDIR}/../../../ofed/include -DLINUX_TYPES_DEFINED

.if !defined(KERNBUILDDIR)
.if ${MK_INET_SUPPORT} != "no"
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}
	echo "#define TCP_OFFLOAD 1" >> ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>
