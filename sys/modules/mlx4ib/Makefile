# $FreeBSD$
.PATH:  ${.CURDIR}/../../ofed/drivers/infiniband/hw/mlx4
.PATH:  ${.CURDIR}/../../ofed/include/linux

.include <bsd.own.mk>

KMOD    = mlx4ib
SRCS    = device_if.h bus_if.h pci_if.h vnode_if.h
SRCS+= linux_compat.c linux_radix.c linux_idr.c 
SRCS+=	alias_GUID.c mcg.c sysfs.c ah.c cq.c doorbell.c mad.c main.c mr.c qp.c srq.c wc.c cm.c
SRCS+=	opt_inet.h opt_inet6.h

#CFLAGS+= -I${.CURDIR}/../../ofed/include/
#CFLAGS+= -I${.CURDIR}/../../../../include
CFLAGS+= -I${.CURDIR}/../../ofed/drivers/infiniband/hw/mlx4
CFLAGS+= -I${.CURDIR}/../../ofed/include/
CFLAGS+= -DCONFIG_INFINIBAND_USER_MEM
CFLAGS+= -DINET6 -DINET -DOFED

.if !defined(KERNBUILDDIR)
.if ${MK_INET_SUPPORT} != "no"
opt_inet.h:
	@echo "#define INET 1" > ${.TARGET}
.endif

.if ${MK_INET6_SUPPORT} != "no"
opt_inet6.h:
	@echo "#define INET6 1" > ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>

CFLAGS+= -Wno-cast-qual -Wno-pointer-arith ${GCC_MS_EXTENSIONS}
