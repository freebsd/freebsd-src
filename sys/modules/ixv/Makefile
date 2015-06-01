#$FreeBSD$

.PATH:  ${.CURDIR}/../../dev/ixgbe

KMOD    = if_ixv
SRCS    = device_if.h bus_if.h pci_if.h
SRCS	+= opt_inet.h opt_inet6.h opt_rss.h
SRCS    += if_ixv.c ix_txrx.c
# Shared source
SRCS    += ixgbe_common.c ixgbe_api.c ixgbe_phy.c
SRCS    += ixgbe_dcb.c ixgbe_mbx.c ixgbe_vf.c
CFLAGS+= -I${.CURDIR}/../../dev/ixgbe -DSMP

.include <bsd.kmod.mk>
