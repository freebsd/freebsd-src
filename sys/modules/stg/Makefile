# $FreeBSD: src/sys/modules/stg/Makefile,v 1.2 2003/04/07 10:13:25 mdodd Exp $

.PATH: ${.CURDIR}/../../dev/stg

KMOD=	stg
SRCS=	tmc18c30.c tmc18c30_subr.c \
	tmc18c30_isa.c		isa_if.h \
	tmc18c30_pccard.c	card_if.h \
	tmc18c30_pci.c		pci_if.h
SRCS+=	opt_bdg.h device_if.h bus_if.h
SRCS+=	opt_cam.h opt_scsi.h opt_ddb.h

.include <bsd.kmod.mk>
