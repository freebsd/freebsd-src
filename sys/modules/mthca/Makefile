# $FreeBSD$
.PATH:  ${.CURDIR}/../../ofed/drivers/infiniband/hw/mthca

KMOD    = mthca
SRCS    = device_if.h bus_if.h pci_if.h vnode_if.h
SRCS+=	mthca_allocator.c mthca_av.c mthca_catas.c mthca_cmd.c mthca_cq.c
SRCS+=	mthca_eq.c mthca_mad.c mthca_main.c mthca_mcg.c mthca_memfree.c
SRCS+=	mthca_mr.c mthca_pd.c mthca_profile.c mthca_provider.c mthca_qp.c
SRCS+=	mthca_reset.c mthca_srq.c mthca_uar.c
SRCS+=	opt_inet.h opt_inet6.h

CFLAGS+= -I${.CURDIR}/../../ofed/include
CFLAGS+= -I${.CURDIR}/../../compat/linuxkpi/common/include

.include <bsd.kmod.mk>

CFLAGS+= -Wno-cast-qual -Wno-pointer-arith
