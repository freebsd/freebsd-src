# $FreeBSD$
.PATH:  ${.CURDIR}/../../ofed/drivers/net/mlx4
KMOD    = mlx4
SRCS    = device_if.h bus_if.h pci_if.h vnode_if.h
SRCS+=	alloc.c catas.c cmd.c cq.c eq.c fw.c icm.c intf.c main.c mcg.c mr.c
SRCS+=	pd.c port.c profile.c qp.c reset.c sense.c srq.c xrcd.c

CFLAGS+= -I${.CURDIR}/../../ofed/drivers/net/mlx4
CFLAGS+= -I${.CURDIR}/../../ofed/include/
CFLAGS+= -DINET6

.include <bsd.kmod.mk>

CFLAGS+= -Wno-cast-qual -Wno-pointer-arith -fms-extensions
