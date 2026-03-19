MT76_DRIVER_NAME=	mt7615

.include <kmod.opts.mk>

.PATH: ${DEVDIR}

MT7615_PCI=		1

MT7615_DEBUGFS=		0
MT7615_DEV_COREDUMP=	0

# Common stuff.
SRCS+=	init.c main.c mac.c mcu.c eeprom.c

# PCIe  (7622_WMAC ignored)
.if defined(MT76_PCI) && ${MT76_PCI} > 0 && ${KERN_OPTS:MDEV_PCI} && \
    defined(MT7615_PCI) && ${MT7615_PCI} > 0
SRCS+=	dma.c mmio.c pci.c pci_init.c pci_mac.c
.endif

# USB + SDIO ignored currently.

.if defined(MT7615_DEBUGFS) && ${MT7615_DEBUGFS} > 0
SRCS+=	debugfs.c
CFLAGS+=	-DCONFIG_MT7915_DEBUGFS
.endif

.if defined(MT7615_DEV_COREDUMP) && ${MT7615_DEV_COREDUMP} > 0
.endif

.include <bsd.kmod.mk>
