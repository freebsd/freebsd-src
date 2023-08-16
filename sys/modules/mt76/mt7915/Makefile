
DEVDIR=	${SRCTOP}/sys/contrib/dev/mediatek/mt76/mt7915

.PATH: ${DEVDIR}

KMOD=	if_mt7915

# Common stuff.
SRCS=	init.c main.c mac.c mcu.c mmio.c eeprom.c
SRCS+=	pci.c dma.c

# SOC+6E stuff.
#SRCS+=	soc.c

.if defined(WITH_DEBUGFS) && ${WITH_DEBUGFS} > 0
SRCS+=	debugfs.c
CFLAGS+=	-DCONFIG_MT7915_DEBUGFS=${WITH_DEBUGFS}
.endif

CFLAGS+=	-DKBUILD_MODNAME='"mt7915"'
CFLAGS+=	-I${DEVDIR}

.include <bsd.kmod.mk>
