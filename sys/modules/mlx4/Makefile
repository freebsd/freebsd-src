# $FreeBSD$
.PATH:  ${.CURDIR}/../../ofed/drivers/net/mlx4
.PATH:  ${.CURDIR}/../../ofed/include/linux
KMOD    = mlx4
SRCS    = device_if.h bus_if.h pci_if.h vnode_if.h
SRCS+= alloc.c catas.c cmd.c cq.c eq.c fw.c icm.c intf.c main.c mcg.c mr.c linux_compat.c linux_radix.c	linux_idr.c
SRCS+=	pd.c port.c profile.c qp.c reset.c sense.c srq.c resource_tracker.c sys_tune.c

CFLAGS+= -I${.CURDIR}/../../ofed/drivers/net/mlx4
CFLAGS+= -I${.CURDIR}/../../ofed/include/

.include <bsd.kmod.mk>

CFLAGS+= -Wno-cast-qual -Wno-pointer-arith ${GCC_MS_EXTENSIONS}
CFLAGS+= -fms-extensions

CWARNFLAGS.mcg.c=	-Wno-unused
CWARNFLAGS+=		${CWARNFLAGS.${.IMPSRC:T}}
