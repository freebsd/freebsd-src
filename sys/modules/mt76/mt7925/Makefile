.include <kmod.opts.mk>

DEVDIR=	${SRCTOP}/sys/contrib/dev/mediatek/mt76/mt7925

.PATH: ${DEVDIR}

KMOD=	if_mt7925

# Common stuff.
SRCS=	init.c main.c mac.c mcu.c regd.c

# PCI parts; PCI needs to be compiled into the kernel and cannot be loaded.
.if defined(WITH_PCI) && ${WITH_PCI} > 0 && ${KERN_OPTS:MDEV_PCI}
SRCS+=	pci.c pci_mac.c pci_mcu.c
.endif

# USB parts; USB can be loaded and is unconditional on any kernel config.
.if defined(WITH_USB) && ${WITH_USB} > 0
SRCS+=	usb.c
.endif

.if defined(WITH_DEBUGFS) && ${WITH_DEBUGFS} > 0
SRCS+=	debugfs.c
CFLAGS+=	-DCONFIG_MT7925_DEBUGFS=${WITH_DEBUGFS}
.endif

CFLAGS+=	-DKBUILD_MODNAME='"mt7925"'
CFLAGS+=	-I${DEVDIR}

.include <bsd.kmod.mk>
