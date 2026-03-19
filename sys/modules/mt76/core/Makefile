MT76_DRIVER_NAME=	mt76_core

.include <kmod.opts.mk>

KMOD=	${MT76_DRIVER_NAME}

.PATH: ${COMMONDIR}

# Basic stuff.
SRCS+=	mac80211.c mmio.c util.c dma.c  eeprom.c tx.c agg-rx.c mcu.c
SRCS+=	channel.c scan.c wed.c

# PCI parts; PCI needs to be compiled into the kernel and cannot be loaded.
.if defined(MT76_PCI) && ${MT76_PCI} > 0 && ${KERN_OPTS:MDEV_PCI}
SRCS+=	pci.c
.endif

# SDIO parts; SDIO depends on an MMCCAM kernel.
.if defined(MT76_SDIO) && ${MT76_SDIO} > 0 && ${KERN_OPTS:MMMCCAM}
SRCS+=	sdio.c sdio_txrx.c
.endif

# USB parts; USB can be loaded and is unconditional on any kernel config.
.if defined(MT76_USB) && ${MT76_USB} > 0
SRCS+=	usb.c
#SRCS+=	usb_trace.c
.endif

# Connac-Lib stuff.
SRCS+=	mt76_connac_mac.c mt76_connac_mcu.c mt76_connac3_mac.c

# MT76x02-Lib stuff (we don't need; that's for older chipsets not yet supported)

# XXX should this be a separate module?
# MT792X-LIB stuff.
SRCS+=	mt792x_core.c mt792x_mac.c mt792x_dma.c
#SRCS+=	mt792x_trace.c
.if defined(MT76_ACPI) && ${MT76_ACPI} > 0
SRCS+=	mt792x_acpi_sar.c
.endif
.if defined(MT76_USB) && ${MT76_USB} > 0
SRCS+=	mt792x_usb.c
.endif
.if defined(MT76_DEBUGFS) && ${MT76_DEBUGFS} > 0
SRCS+=	mt792x_debugfs.c
.endif

.if defined(MT76_DEBUGFS) && ${MT76_DEBUGFS} > 0
SRCS+=	debugfs.c
.endif

#SRCS+=	trace.c

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
