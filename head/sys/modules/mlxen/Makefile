# $FreeBSD$
.PATH:  ${.CURDIR}/../../ofed/drivers/net/mlx4
KMOD    = mlxen
SRCS    = device_if.h bus_if.h pci_if.h vnode_if.h
SRCS	+= en_cq.c en_frag.c en_main.c en_netdev.c en_port.c en_resources.c
SRCS	+= en_rx.c en_tx.c
CFLAGS+= -I${.CURDIR}/../../ofed/drivers/net/mlx4
CFLAGS+= -I${.CURDIR}/../../ofed/include/
CFLAGS+= -DINET6

.include <bsd.kmod.mk>

CFLAGS+= -Wno-cast-qual -Wno-pointer-arith -fms-extensions
