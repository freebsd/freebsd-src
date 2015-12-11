# $FreeBSD$
.PATH:  ${.CURDIR}/../../ofed/drivers/net/mlx4

.include <bsd.own.mk>

KMOD    = mlxen
SRCS    = device_if.h bus_if.h pci_if.h vnode_if.h
SRCS	+= en_cq.c en_main.c en_netdev.c en_port.c en_resources.c
SRCS	+= en_rx.c en_tx.c utils.c
SRCS	+= opt_inet.h opt_inet6.h
CFLAGS+= -I${.CURDIR}/../../ofed/drivers/net/mlx4
CFLAGS+= -I${.CURDIR}/../../ofed/include/
CFLAGS+= -fms-extensions

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

CFLAGS+= ${GCC_MS_EXTENSIONS}
