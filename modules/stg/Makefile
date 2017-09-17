# $FreeBSD$

.PATH: ${SRCTOP}/sys/dev/stg

KMOD=	stg
SRCS=	tmc18c30.c tmc18c30_subr.c \
	tmc18c30_isa.c		isa_if.h \
	tmc18c30_pccard.c	card_if.h pccarddevs.h \
	tmc18c30_pci.c		pci_if.h
SRCS+=	device_if.h bus_if.h
SRCS+=	opt_cam.h opt_scsi.h opt_ddb.h

.include <bsd.kmod.mk>
