#$FreeBSD$
.PATH:  ${.CURDIR}/../../dev/ixgb
KMOD    = if_ixgb
SRCS    = device_if.h bus_if.h pci_if.h opt_bdg.h
SRCS    += if_ixgb.c ixgb_hw.c ixgb_ee.c
clean:
	rm -f opt_bdg.h device_if.h bus_if.h pci_if.h setdef*
	rm -f *.o *.kld *.ko
	rm -f @ machine

.include <bsd.kmod.mk>
