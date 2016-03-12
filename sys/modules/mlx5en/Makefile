# $FreeBSD$
.PATH:	${.CURDIR}/../../dev/mlx5/mlx5_en

KMOD=mlx5en
SRCS= \
mlx5_en_ethtool.c \
mlx5_en_main.c \
mlx5_en_tx.c \
mlx5_en_flow_table.c \
mlx5_en_rx.c \
mlx5_en_txrx.c \
device_if.h bus_if.h vnode_if.h pci_if.h \
        opt_inet.h opt_inet6.h opt_rss.h

.if defined(HAVE_TURBO_LRO)
CFLAGS+= -DHAVE_TURBO_LRO
SRCS+= tcp_tlro.c
.endif

.if defined(HAVE_PER_CQ_EVENT_PACKET)
CFLAGS+= -DHAVE_PER_CQ_EVENT_PACKET
.endif

CFLAGS+= -I${.CURDIR}/../../ofed/include
CFLAGS+= -I${.CURDIR}/../../compat/linuxkpi/common/include

.include <bsd.kmod.mk>

CFLAGS+= -Wno-cast-qual -Wno-pointer-arith ${GCC_MS_EXTENSIONS}
