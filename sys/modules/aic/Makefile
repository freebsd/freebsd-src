# $FreeBSD: src/sys/modules/aic/Makefile,v 1.6 2004/05/26 00:53:00 imp Exp $

.PATH: ${.CURDIR}/../../dev/aic

KMOD=	aic
SRCS=	aic.c aic_pccard.c
SRCS+=	device_if.h bus_if.h pci_if.h isa_if.h card_if.h pccarddevs.h
SRCS+=	opt_cam.h opt_scsi.h

.if ${MACHINE} == "pc98"
SRCS+=	aic_cbus.c
.else
SRCS+=	aic_isa.c
.endif

.include <bsd.kmod.mk>
