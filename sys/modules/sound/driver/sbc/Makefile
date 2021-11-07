# $FreeBSD$

.PATH: ${SRCTOP}/sys/dev/sound/isa

KMOD=	snd_sbc
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h
SRCS+=	sbc.c

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
