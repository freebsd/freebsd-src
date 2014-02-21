# $FreeBSD$

.PATH:  ${.CURDIR}/../../ofed/drivers/infiniband/hw/mthca

.include <bsd.own.mk>

KMOD    = mthca
SRCS    = device_if.h bus_if.h pci_if.h vnode_if.h
SRCS+=	mthca_allocator.c mthca_av.c mthca_catas.c mthca_cmd.c mthca_cq.c
SRCS+=	mthca_eq.c mthca_mad.c mthca_main.c mthca_mcg.c mthca_memfree.c
SRCS+=	mthca_mr.c mthca_pd.c mthca_profile.c mthca_provider.c mthca_qp.c
SRCS+=	mthca_reset.c mthca_srq.c mthca_uar.c
SRCS+=	opt_inet.h opt_inet6.h

CFLAGS+= -I${.CURDIR}/../../ofed/include

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

CFLAGS+= -Wno-cast-qual -Wno-pointer-arith ${GCC_MS_EXTENSIONS}
