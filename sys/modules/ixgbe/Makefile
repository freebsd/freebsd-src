#$FreeBSD: src/sys/modules/ixgbe/Makefile,v 1.1.2.1.4.1 2010/02/10 00:26:20 kensmith Exp $
.PATH:  ${.CURDIR}/../../dev/ixgbe
KMOD    = ixgbe
SRCS    = device_if.h bus_if.h pci_if.h opt_bdg.h
SRCS    += ixgbe.c 
# Shared source
SRCS    += ixgbe_common.c ixgbe_api.c ixgbe_phy.c
SRCS    += ixgbe_82598.c ixgbe_82599.c

CFLAGS+= -I${.CURDIR}/../../dev/ixgbe

clean:
	rm -f opt_bdg.h device_if.h bus_if.h pci_if.h setdef* *_StripErr
	rm -f *.o *.kld *.ko
	rm -f @ machine

.include <bsd.kmod.mk>
