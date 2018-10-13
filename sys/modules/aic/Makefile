# $FreeBSD$

.PATH: ${SRCTOP}/sys/dev/aic

KMOD=	aic
SRCS=	aic.c aic_isa.c aic_pccard.c
SRCS+=	device_if.h bus_if.h pci_if.h isa_if.h card_if.h pccarddevs.h
SRCS+=	opt_cam.h opt_scsi.h

.include <bsd.kmod.mk>
