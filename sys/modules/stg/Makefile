# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/stg

KMOD=	stg
SRCS=	tmc18c30.c tmc18c30_isa.c tmc18c30_pccard.c
SRCS+=	opt_bdg.h device_if.h bus_if.h pci_if.h isa_if.h card_if.h
SRCS+=	opt_cam.h opt_scsi.h opt_ddb.h

.include <bsd.kmod.mk>
