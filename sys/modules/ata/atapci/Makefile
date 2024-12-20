SUBDIR += chipsets

.PATH: ${SRCTOP}/sys/dev/ata

KMOD=	atapci
SRCS=	ata-pci.c ata-dma.c ata-sata.c
SRCS+=	ata_if.h bus_if.h device_if.h pci_if.h

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
