# $FreeBSD$
.PATH:  ${.CURDIR}/../../dev/mlx4/mlx4_core

KMOD=	mlx4
SRCS=	device_if.h bus_if.h vnode_if.h pci_if.h \
	opt_inet.h opt_inet6.h \
	mlx4_alloc.c \
	mlx4_catas.c \
	mlx4_cmd.c \
	mlx4_cq.c \
	mlx4_eq.c \
	mlx4_fw.c \
	mlx4_icm.c \
	mlx4_intf.c \
	mlx4_main.c \
	mlx4_mcg.c \
	mlx4_mr.c \
	mlx4_pd.c \
	mlx4_port.c \
	mlx4_profile.c \
	mlx4_qp.c \
	mlx4_reset.c \
	mlx4_sense.c \
	mlx4_srq.c \
	mlx4_resource_tracker.c \
	mlx4_sys_tune.c

CFLAGS+= -I${.CURDIR}/../../ofed/include
CFLAGS+= -I${.CURDIR}/../../compat/linuxkpi/common/include

.include <bsd.kmod.mk>

CFLAGS+= -Wno-cast-qual -Wno-pointer-arith

CWARNFLAGS.mlx4_mcg.c=	-Wno-unused
