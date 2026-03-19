MT76_DRIVER_NAME=	mt7925

.include <kmod.opts.mk>

.PATH: ${DEVDIR}

MT7925_PCI=		1
MT7925_USB=		1

MT7925_DEBUGFS=		0

# Common stuff.
SRCS+=	init.c main.c mac.c mcu.c regd.c

# PCI parts; PCI needs to be compiled into the kernel and cannot be loaded.
.if defined(MT76_PCI) && ${MT76_PCI} > 0 && ${KERN_OPTS:MDEV_PCI} && \
    defined(MT7925_PCI) && ${MT7925_PCI} > 0
SRCS+=	pci.c pci_mac.c pci_mcu.c
.endif

# USB parts; USB can be loaded and is unconditional on any kernel config.
.if defined(MT76_USB) && ${MT76_USB} > 0 && \
    defined(MT7925_USB) && ${MT7925_USB} > 0
SRCS+=	usb.c
.endif

.if defined(MT7925_DEBUGFS) && ${MT7925_DEBUGFS} > 0
SRCS+=	debugfs.c
CFLAGS+=	-DCONFIG_MT7925_DEBUGFS
.endif

.include <bsd.kmod.mk>
