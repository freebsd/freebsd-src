# $FreeBSD$
.PATH:  ${.CURDIR}/../../ofed/drivers/infiniband/ulp/ipoib
.PATH:  ${.CURDIR}/../../ofed/include/linux

KMOD    = ipoib
SRCS    = device_if.h bus_if.h opt_ofed.h vnode_if.h opt_inet.h opt_inet6.h
SRCS    += ipoib_cm.c ipoib_ib.c ipoib_main.c ipoib_multicast.c ipoib_verbs.c ipoib.h
SRCS+=  linux_compat.c linux_radix.c linux_idr.c

CFLAGS+= -I${.CURDIR}/../../ofed/drivers/infiniband/ulp/ipoib
CFLAGS+= -I${.CURDIR}/../ibcore
CFLAGS+= -I${.CURDIR}/../../ofed/include/
CFLAGS+= -DINET6 -DINET -DOFED

.include <bsd.kmod.mk>

CFLAGS+= -Wno-cast-qual -Wno-pointer-arith ${GCC_MS_EXTENSIONS}
