#$FreeBSD$

.PATH:  ${.CURDIR}/../../dev/igb
KMOD    = if_igb
SRCS    = device_if.h bus_if.h pci_if.h opt_inet.h opt_inet6.h opt_rss.h
SRCS    += if_igb.c $(SHARED_SRCS)
SHARED_SRCS = igb_api.c igb_phy.c igb_nvm.c igb_mac.c igb_manage.c
SHARED_SRCS += igb_82575.c igb_vf.c igb_mbx.c igb_i210.c

CFLAGS += -I${.CURDIR}/../../dev/igb -DSMP

# DEVICE_POLLING gives you non-interrupt handling
# not advisable since MSIX gives better results
#CFLAGS  += -DDEVICE_POLLING

# IGB_LEGACY_TX will override the stack if_transmit path and
# instead use the older if_start non-multiqueue capable interface.
# This might be desireable for testing, or to enable the use of
# ALTQ. 
#CFLAGS  += -DIGB_LEGACY_TX

.include <bsd.kmod.mk>
