# $FreeBSD: src/sys/modules/ncv/Makefile,v 1.1 2001/12/15 12:37:36 non Exp $

.PATH: ${.CURDIR}/../../dev/ncv

KMOD=	ncv
SRCS=	ncr53c500_pccard.c ncr53c500.c 
SRCS+=	opt_bdg.h device_if.h bus_if.h pci_if.h isa_if.h card_if.h
SRCS+=	opt_cam.h opt_scsi.h opt_ddb.h

.include <bsd.kmod.mk>
