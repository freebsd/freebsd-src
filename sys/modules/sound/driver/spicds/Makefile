.PATH: ${SRCTOP}/sys/dev/sound/pci

KMOD=	snd_spicds
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	spicds.c

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
