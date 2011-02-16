# $FreeBSD: src/sys/modules/stg/Makefile,v 1.4.14.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/stg

KMOD=	stg
SRCS=	tmc18c30.c tmc18c30_subr.c \
	tmc18c30_isa.c		isa_if.h \
	tmc18c30_pccard.c	card_if.h pccarddevs.h \
	tmc18c30_pci.c		pci_if.h
SRCS+=	device_if.h bus_if.h
SRCS+=	opt_cam.h opt_scsi.h opt_ddb.h

.include <bsd.kmod.mk>
