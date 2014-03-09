#$FreeBSD$

.include <bsd.own.mk>

.PATH:  ${.CURDIR}/../../dev/ixgbe

KMOD    = if_ixgbe
SRCS    = device_if.h bus_if.h pci_if.h
SRCS	+= opt_inet.h opt_inet6.h
SRCS    += ixgbe.c ixv.c
# Shared source
SRCS    += ixgbe_common.c ixgbe_api.c ixgbe_phy.c ixgbe_mbx.c ixgbe_vf.c
SRCS    += ixgbe_dcb.c ixgbe_dcb_82598.c ixgbe_dcb_82599.c
SRCS    += ixgbe_82599.c ixgbe_82598.c ixgbe_x540.c
CFLAGS+= -I${.CURDIR}/../../dev/ixgbe -DSMP -DIXGBE_FDIR

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
