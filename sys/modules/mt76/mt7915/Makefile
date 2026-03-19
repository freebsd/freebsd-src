MT76_DRIVER_NAME=	mt7915

.include <kmod.opts.mk>

.PATH: ${DEVDIR}

MT7915_PCI=		1

MT7915_WMAC=		0
MT7915_DEBUGFS=		0
MT7915_DEV_COREDUMP=	1

SRCS+=	init.c main.c mac.c mcu.c mmio.c eeprom.c

# PCI only driver so it better be there (otherwise nothing will attach at least).
.if defined(MT76_PCI) && ${MT76_PCI} > 0 && ${KERN_OPTS:MDEV_PCI} && \
    defined(MT7915_PCI) && ${MT7915_PCI} > 0
SRCS+=	pci.c dma.c
.endif

# SOC+6E stuff.
.if defined(MT7915_WMAC) && ${MT7915_WMAC} > 0
SRCS+=	soc.c
CFLAGS+=	-DCONFIG_MT798X_WMAC
.endif

.if defined(MT7915_DEBUGFS) && ${MT7915_DEBUGFS} > 0
SRCS+=	debugfs.c
CFLAGS+=	-DCONFIG_MT7915_DEBUGFS
.endif

.if defined(MT7915_DEV_COREDUMP) && ${MT7915_DEV_COREDUMP} > 0
SRCS+=	coredump.c
CFLAGS+=	-DCONFIG_DEV_COREDUMP
.endif

.include <bsd.kmod.mk>
