DEVDIR=	${SRCTOP}/sys/contrib/dev/mediatek/mt76/mt7615

.PATH: ${DEVDIR}

WITH_DEBUGFS=		0
WITH_DEV_COREDUMP=	0

KMOD=	if_mt7615

# Common stuff.
SRCS=	init.c main.c mac.c mcu.c eeprom.c

# PCIe  (7622_WMAC ignored)
SRCS+=	dma.c mmio.c pci.c pci_init.c pci_mac.c

# USB + SDIO ignored currently.

.if defined(WITH_DEBUGFS) && ${WITH_DEBUGFS} > 0
SRCS+=	debugfs.c
CFLAGS+=	-DCONFIG_MT7915_DEBUGFS=${WITH_DEBUGFS}
.endif

CFLAGS+=	-DKBUILD_MODNAME='"mt7615"'
CFLAGS+=	-I${DEVDIR}

.include <bsd.kmod.mk>
