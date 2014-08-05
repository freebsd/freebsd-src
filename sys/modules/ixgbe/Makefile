#$FreeBSD$

.PATH:  ${.CURDIR}/../../dev/ixgbe

KMOD    = if_ixgbe
SRCS    = device_if.h bus_if.h pci_if.h
SRCS	+= opt_inet.h opt_inet6.h opt_rss.h
SRCS    += ixgbe.c ixv.c
# Shared source
SRCS    += ixgbe_common.c ixgbe_api.c ixgbe_phy.c ixgbe_mbx.c ixgbe_vf.c
SRCS    += ixgbe_dcb.c ixgbe_dcb_82598.c ixgbe_dcb_82599.c
SRCS    += ixgbe_82599.c ixgbe_82598.c ixgbe_x540.c
CFLAGS+= -I${.CURDIR}/../../dev/ixgbe -DSMP -DIXGBE_FDIR

.include <bsd.kmod.mk>
