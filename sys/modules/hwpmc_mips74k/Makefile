#
# $FreeBSD$
#

.PATH: ${SRCTOP}/sys/dev/hwpmc

KMOD=	hwpmc_mips74k

SRCS=	bus_if.h device_if.h vnode_if.h
SRCS+=	hwpmc_mod.c hwpmc_logging.c hwpmc_soft.c
SRCS+=	hwpmc_mips.c

SRCS+=	hwpmc_mips74k.c

SRCS+=	opt_hwpmc_hooks.h

.include <bsd.kmod.mk>
