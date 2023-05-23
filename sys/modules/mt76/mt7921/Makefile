
DEVDIR=	${SRCTOP}/sys/contrib/dev/mediatek/mt76/mt7921

.PATH: ${DEVDIR}

KMOD=	if_mt7921

# Common stuff.
SRCS=	init.c main.c mac.c mcu.c

# PCI stuff.
SRCS+=	pci.c pci_mac.c pci_mcu.c

# USB stuff.
#SRCS+=	usb.c

# SDIO stuff.
#SRCS+=	sdio.c sdio_mac.c sdio_mcu.c

.if defined(WITH_DEBUGFS) && ${WITH_DEBUGFS} > 0
SRCS+=	debugfs.c
CFLAGS+=	-DCONFIG_MT7921_DEBUGFS=${WITH_DEBUGFS}
.endif

CFLAGS+=	-DKBUILD_MODNAME='"mt7921"'
CFLAGS+=	-I${DEVDIR}

.include <bsd.kmod.mk>
