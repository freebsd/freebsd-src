# $FreeBSD: src/sys/modules/aic/Makefile,v 1.6.24.1 2008/10/02 02:57:24 kensmith Exp $

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
