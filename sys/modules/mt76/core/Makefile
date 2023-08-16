
KMOD=	mt76_core

# Basic stuff.
SRCS=	mac80211.c mmio.c util.c dma.c  eeprom.c tx.c agg-rx.c mcu.c
#SRCS+=	trace.c

# Bus stuff.
SRCS+=	pci.c
#SRCS+=	usb.c	# usb_trace.c
#SRCS+=	sdio.c sdio_txrx.c

# Connac-Lib stuff.
SRCS+=	mt76_connac_mac.c mt76_connac_mcu.c

# MT76x02-Lib stuff (we don't need; that's for older chipsets not yet supported)

.if defined(WITH_DEBUGFS) && ${WITH_DEBUGFS} > 0
SRCS+=	debugfs.c
.endif

CFLAGS+=	-DKBUILD_MODNAME='"mt76_core"'
CFLAGS+=	-DCONFIG_MAC80211_DEBUGFS=${WITH_DEBUGFS}

.include <bsd.kmod.mk>
