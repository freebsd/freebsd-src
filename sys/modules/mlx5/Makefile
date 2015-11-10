# $FreeBSD$
.PATH:	${.CURDIR}/../../dev/mlx5/mlx5_core

KMOD=mlx5
SRCS= \
mlx5_alloc.c \
mlx5_cmd.c \
mlx5_cq.c \
mlx5_eq.c \
mlx5_flow_table.c \
mlx5_fw.c \
mlx5_health.c \
mlx5_mad.c \
mlx5_main.c \
mlx5_mcg.c \
mlx5_mr.c \
mlx5_pagealloc.c \
mlx5_pd.c \
mlx5_port.c \
mlx5_qp.c \
mlx5_srq.c \
mlx5_transobj.c \
mlx5_uar.c \
mlx5_vport.c \
mlx5_wq.c \
device_if.h bus_if.h vnode_if.h pci_if.h \
        opt_inet.h opt_inet6.h opt_random.h

CFLAGS+= -I${.CURDIR}/../../ofed/include
CFLAGS+= -I${.CURDIR}/../../compat/linuxkpi/common/include

.include <bsd.kmod.mk>

CFLAGS+= -Wno-cast-qual -Wno-pointer-arith ${GCC_MS_EXTENSIONS}
