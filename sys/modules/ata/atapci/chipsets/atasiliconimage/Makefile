.PATH: ${SRCTOP}/sys/dev/ata/chipsets

KMOD=	atasiliconimage
SRCS=	ata-siliconimage.c
SRCS+=	ata_if.h bus_if.h device_if.h pci_if.h

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
