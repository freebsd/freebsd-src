#$FreeBSD$
.PATH:  ${.CURDIR}/../../dev/ixgbe
KMOD    = ixgbe
SRCS    = device_if.h bus_if.h pci_if.h opt_bdg.h
SRCS    += ixgbe.c 
# Shared source
SRCS    += ixgbe_common.c ixgbe_api.c ixgbe_phy.c
SRCS    += ixgbe_82598.c ixgbe_82599.c

CFLAGS+= -I${.CURDIR}/../../dev/ixgbe

.include <bsd.kmod.mk>
