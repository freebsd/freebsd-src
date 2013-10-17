# $FreeBSD$

.include <bsd.own.mk>

CXGBE = ${.CURDIR}/../../../dev/cxgbe
.PATH: ${CXGBE}/iw_cxgbe

KMOD=   iw_cxgbe
SRCS=   device.c cm.c provider.c mem.c cq.c qp.c resource.c ev.c id_table.c
SRCS+=  bus_if.h device_if.h opt_sched.h pci_if.h pcib_if.h opt_ktr.h
SRCS+=  opt_inet.h opt_ofed.h vnode_if.h
CFLAGS+= -I${CXGBE} -I${.CURDIR}/../../../ofed/include -DLINUX_TYPES_DEFINED

.if !defined(KERNBUILDDIR)
.if ${MK_INET_SUPPORT} != "no"
opt_inet.h:
	@echo "#define INET 1" > ${.TARGET}
	@echo "#define TCP_OFFLOAD 1" >> ${.TARGET}
.endif

.if ${MK_INET6_SUPPORT} != "no"
opt_inet6.h:
	@echo "#define INET6 1" > ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>
