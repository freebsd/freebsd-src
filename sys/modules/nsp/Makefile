# $FreeBSD: src/sys/modules/nsp/Makefile,v 1.1 2001/12/15 12:37:36 non Exp $

.PATH: ${.CURDIR}/../../dev/nsp

KMOD=	nsp
SRCS=	nsp.c nsp_pccard.c
SRCS+=	opt_bdg.h device_if.h bus_if.h pci_if.h isa_if.h card_if.h
SRCS+=	opt_cam.h opt_scsi.h opt_ddb.h

.include <bsd.kmod.mk>
