.PATH: ${SRCTOP}/sys/dev/enetc

KMOD	= if_enetc
SRCS	= if_enetc.c enetc_mdio.c
SRCS	+= bus_if.h device_if.h ifdi_if.h miibus_if.h ofw_bus_if.h pci_if.h opt_platform.h

.include <bsd.kmod.mk>
