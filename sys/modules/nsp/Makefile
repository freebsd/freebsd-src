# $FreeBSD: src/sys/modules/nsp/Makefile,v 1.3.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/nsp

KMOD=	nsp
SRCS=	nsp.c nsp_pccard.c
SRCS+=	device_if.h bus_if.h pci_if.h isa_if.h card_if.h pccarddevs.h
SRCS+=	opt_cam.h opt_scsi.h opt_ddb.h

.include <bsd.kmod.mk>
