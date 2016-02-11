#$FreeBSD$

.PATH:  ${.CURDIR}/../../dev/ixgbe

KMOD    = if_ix
SRCS    = device_if.h bus_if.h pci_if.h
SRCS	+= opt_inet.h opt_inet6.h
SRCS    += if_ix.c ix_txrx.c ixgbe_osdep.c
# Shared source
SRCS    += ixgbe_common.c ixgbe_api.c ixgbe_phy.c ixgbe_mbx.c ixgbe_vf.c
SRCS    += ixgbe_dcb.c ixgbe_dcb_82598.c ixgbe_dcb_82599.c
SRCS    += ixgbe_82598.c ixgbe_82599.c ixgbe_x540.c ixgbe_x550.c
CFLAGS+= -I${.CURDIR}/../../dev/ixgbe -DSMP

.include <bsd.kmod.mk>
