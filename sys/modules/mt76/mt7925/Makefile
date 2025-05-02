DEVDIR=	${SRCTOP}/sys/contrib/dev/mediatek/mt76/mt7925

.PATH: ${DEVDIR}

KMOD=	if_mt7925

# Common stuff.
SRCS=	init.c main.c mac.c mcu.c

# PCI stuff.
SRCS+=	pci.c pci_mac.c pci_mcu.c

# USB stuff.
#SRCS+=	usb.c

.if defined(WITH_DEBUGFS) && ${WITH_DEBUGFS} > 0
SRCS+=	debugfs.c
CFLAGS+=	-DCONFIG_MT7925_DEBUGFS=${WITH_DEBUGFS}
.endif

CFLAGS+=	-DKBUILD_MODNAME='"mt7925"'
CFLAGS+=	-I${DEVDIR}

.include <bsd.kmod.mk>
