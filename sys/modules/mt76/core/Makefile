
KMOD=	mt76_core

# Basic stuff.
SRCS=	mac80211.c mmio.c util.c dma.c  eeprom.c tx.c agg-rx.c mcu.c
#SRCS+=	trace.c

# Bus stuff.
SRCS+=	pci.c
.if defined(WITH_USB) && ${WITH_USB} > 0
SRCS+=	usb.c	# usb_trace.c
.endif
#SRCS+=	sdio.c sdio_txrx.c

# Connac-Lib stuff.
SRCS+=	mt76_connac_mac.c mt76_connac_mcu.c mt76_connac3_mac.c

# MT76x02-Lib stuff (we don't need; that's for older chipsets not yet supported)

# XXX should this be a separate module?
# MT792X-LIB stuff.
SRCS+=	mt792x_core.c mt792x_mac.c mt792x_dma.c
#SRCS+=	mt792x_trace.c
.if defined(WITH_ACPI) && ${WITH_ACPI} > 0
SRCS+=	mt792x_acpi_sar.c
.endif
.if defined(WITH_USB) && ${WITH_USB} > 0
SRCS+=	mt792x_usb.c
.endif
.if defined(WITH_DEBUGFS) && ${WITH_DEBUGFS} > 0
SRCS+=	mt792x_debugfs.c
.endif

.if defined(WITH_DEBUGFS) && ${WITH_DEBUGFS} > 0
SRCS+=	debugfs.c
.endif

CFLAGS+=	-DKBUILD_MODNAME='"mt76_core"'
CFLAGS+=	-DCONFIG_MAC80211_DEBUGFS=${WITH_DEBUGFS}

.include <bsd.kmod.mk>
