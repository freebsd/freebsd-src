
.PATH:  ${SRCTOP}/sys/dev/axgbe

KMOD    = if_axp
SRCS    = device_if.h bus_if.h miibus_if.h pci_if.h ifdi_if.h vnode_if.h
SRCS	+= opt_inet.h opt_inet6.h opt_rss.h opt_iflib.h opt_platform.h
SRCS    += if_axgbe_pci.c xgbe-sysctl.c xgbe_osdep.c xgbe-desc.c xgbe-drv.c xgbe-mdio.c xgbe-dev.c xgbe-i2c.c xgbe-phy-v2.c xgbe-txrx.c

CFLAGS+= ${LINUXKPI_INCLUDES}

.include <bsd.kmod.mk>
