MT76_DRIVER_NAME=	mt7996

.include <kmod.opts.mk>

.PATH: ${DEVDIR}

MT7996_PCI=		1

MT7996_DEBUGFS=		0
MT7996_DEV_COREDUMP=	1
MT7996_NPU=		0

# Common stuff.
SRCS+=	init.c main.c mac.c mcu.c mmio.c eeprom.c
# PCI only driver so it better be there (otherwise nothing will attach at least).
.if defined(MT76_PCI) && ${MT76_PCI} > 0 && ${KERN_OPTS:MDEV_PCI} && \
    defined(MT7996_PCI) && ${MT7996_PCI} > 0
SRCS+=	pci.c dma.c
.endif

.if defined(MT7996_DEBUGFS) && ${MT7996_DEBUGFS} > 0
SRCS+=	debugfs.c
CFLAGS+=	-DCONFIG_MT7996_DEBUGFS
.endif

.if defined(MT7996_DEV_COREDUMP) && ${MT7996_DEV_COREDUMP} > 0
SRCS+=	coredump.c
CFLAGS+=	-DCONFIG_DEV_COREDUMP
.endif

.if defined(MT7996_NPU) && ${MT7996_NPU} > 0
SRCS+=	npu.c
CFLAGS+=	-DCONFIG_MT7996_NPU
.endif

.include <bsd.kmod.mk>
