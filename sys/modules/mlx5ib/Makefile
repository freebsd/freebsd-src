# $FreeBSD$
.PATH:	${SRCTOP}/sys/dev/mlx5/mlx5_ib

KMOD=mlx5ib
SRCS= \
mlx5_ib_ah.c \
mlx5_ib_cq.c \
mlx5_ib_doorbell.c \
mlx5_ib_mad.c \
mlx5_ib_main.c \
mlx5_ib_mem.c \
mlx5_ib_mr.c \
mlx5_ib_qp.c \
mlx5_ib_roce.c \
mlx5_ib_srq.c \
device_if.h bus_if.h vnode_if.h pci_if.h \
opt_inet.h opt_inet6.h

CFLAGS+= -I${SRCTOP}/sys/ofed/include
CFLAGS+= -I${SRCTOP}/sys/compat/linuxkpi/common/include

.include <bsd.kmod.mk>

CFLAGS+= -Wno-cast-qual -Wno-pointer-arith ${GCC_MS_EXTENSIONS}
