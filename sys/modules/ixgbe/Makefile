#$FreeBSD: src/sys/modules/ixgbe/Makefile,v 1.6.2.4.2.1 2010/12/21 17:09:25 kensmith Exp $
.PATH:  ${.CURDIR}/../../dev/ixgbe
KMOD    = ixgbe
SRCS    = device_if.h bus_if.h pci_if.h
SRCS    += ixgbe.c ixv.c
# Shared source
SRCS    += ixgbe_common.c ixgbe_api.c ixgbe_phy.c ixgbe_mbx.c ixgbe_vf.c
SRCS    += ixgbe_82599.c ixgbe_82598.c
CFLAGS+= -I${.CURDIR}/../../dev/ixgbe -DSMP -DIXGBE_FDIR

.include <bsd.kmod.mk>
