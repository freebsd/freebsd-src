# $FreeBSD: src/sys/modules/aic/Makefile,v 1.5 2002/11/06 13:46:58 nyan Exp $

.PATH: ${.CURDIR}/../../dev/aic

KMOD=	aic
SRCS=	aic.c aic_pccard.c
SRCS+=	opt_bdg.h device_if.h bus_if.h pci_if.h isa_if.h card_if.h
SRCS+=	opt_cam.h opt_scsi.h

.if ${MACHINE} == "pc98"
SRCS+=	aic_cbus.c
.else
SRCS+=	aic_isa.c
.endif

.include <bsd.kmod.mk>
