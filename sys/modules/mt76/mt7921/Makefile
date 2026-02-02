.include <kmod.opts.mk>

DEVDIR=	${SRCTOP}/sys/contrib/dev/mediatek/mt76/mt7921

.PATH: ${DEVDIR}

KMOD=	if_mt7921

# Common stuff.
SRCS=	init.c main.c mac.c mcu.c

# PCI parts; PCI needs to be compiled into the kernel and cannot be loaded.
.if defined(WITH_PCI) && ${WITH_PCI} > 0 && ${KERN_OPTS:MDEV_PCI}
SRCS+=	pci.c pci_mac.c pci_mcu.c
.endif

# USB parts; USB can be loaded and is unconditional on any kernel config.
.if defined(WITH_USB) && ${WITH_USB} > 0
SRCS+=	usb.c
.endif

# SDIO parts; SDIO depends on an MMCCAM kernel.
.if defined(WITH_SDIO) && ${WITH_SDIO} > 0 && ${KERN_OPTS:MMMCCAM}
SRCS+=	sdio.c sdio_mac.c sdio_mcu.c
.endif

.if defined(WITH_DEBUGFS) && ${WITH_DEBUGFS} > 0
SRCS+=	debugfs.c
CFLAGS+=	-DCONFIG_MT7921_DEBUGFS=${WITH_DEBUGFS}
.endif

CFLAGS+=	-DKBUILD_MODNAME='"mt7921"'
CFLAGS+=	-I${DEVDIR}

.include <bsd.kmod.mk>
