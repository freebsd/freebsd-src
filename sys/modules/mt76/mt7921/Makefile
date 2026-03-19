MT76_DRIVER_NAME=	mt7921

.include <kmod.opts.mk>

.PATH: ${DEVDIR}

MT7921_PCI=		1
MT7921_SDIO=		0
MT7921_USB=		0

MT7921_DEBUGFS=		0

# Common stuff.
SRCS+=	init.c main.c mac.c mcu.c

# PCI parts; PCI needs to be compiled into the kernel and cannot be loaded.
.if defined(MT76_PCI) && ${MT76_PCI} > 0 && ${KERN_OPTS:MDEV_PCI} && \
    defined(MT7921_PCI) && ${MT7921_PCI} > 0
SRCS+=	pci.c pci_mac.c pci_mcu.c
.endif

# SDIO parts; SDIO depends on an MMCCAM kernel.
.if defined(MT76_SDIO) && ${MT76_SDIO} > 0 && ${KERN_OPTS:MMMCCAM} && \
    defined(MT7921_SDIO) && ${MT7921_SDIO} > 0
SRCS+=	sdio.c sdio_mac.c sdio_mcu.c
.endif

# USB parts; USB can be loaded and is unconditional on any kernel config.
.if defined(MT76_USB) && ${MT76_USB} > 0 && \
    defined(MT7921_USB) && ${MT7921_USB} > 0
SRCS+=	usb.c
.endif

.if defined(MT7921_DEBUGFS) && ${MT7921_DEBUGFS} > 0
SRCS+=	debugfs.c
CFLAGS+=	-DCONFIG_MT7921_DEBUGFS
.endif

.include <bsd.kmod.mk>
