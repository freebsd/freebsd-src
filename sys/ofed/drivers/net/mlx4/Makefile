# $FreeBSD$
#.PATH:  ${.CURDIR}/../../ofed/drivers/net/mlx4:${.CURDIR}/../../ofed/include/linux
.PATH:  ${.CURDIR}/../../../../../include/linux

.include <bsd.own.mk>


KMOD    = mlx4
SRCS    = device_if.h bus_if.h pci_if.h vnode_if.h 
SRCS+=  alloc.c catas.c cmd.c cq.c eq.c fw.c icm.c intf.c main.c mcg.c mr.c linux_compat.c linux_radix.c
SRCS+=  pd.c port.c profile.c qp.c reset.c sense.c srq.c resource_tracker.c sys_tune.c
SRCS+=  opt_inet.h opt_inet6.h


#CFLAGS+= -I${.CURDIR}/../../ofed/drivers/net/mlx4
#CFLAGS+= -I${.CURDIR}/../../ofed/include/
CFLAGS+= -I${.CURDIR}/../../../../../include

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

