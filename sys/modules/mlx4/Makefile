# $FreeBSD$
.PATH:  ${.CURDIR}/../../ofed/drivers/net/mlx4

KMOD=	mlx4
SRCS=	device_if.h bus_if.h vnode_if.h pci_if.h \
	opt_inet.h opt_inet6.h \
	alloc.c catas.c cmd.c cq.c eq.c \
	fw.c icm.c intf.c main.c mcg.c mr.c \
	pd.c port.c profile.c qp.c reset.c sense.c \
	srq.c resource_tracker.c sys_tune.c

CFLAGS+= -I${.CURDIR}/../../ofed/drivers/net/mlx4
CFLAGS+= -I${.CURDIR}/../../ofed/include
CFLAGS+= -I${.CURDIR}/../../compat/linuxkpi/common/include

.include <bsd.kmod.mk>

CFLAGS+= -Wno-cast-qual -Wno-pointer-arith

CWARNFLAGS.mcg.c=	-Wno-unused
