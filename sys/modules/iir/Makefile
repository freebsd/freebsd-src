# $FreeBSD: src/sys/modules/iir/Makefile,v 1.1.2.1 2002/05/04 08:16:15 asmodai Exp $

KMOD=   iir

.PATH: ${.CURDIR}/../../dev/${KMOD}

SRCS=   iir.c iir_ctrl.c iir_pci.c
SRCS+=  opt_scsi.h opt_cam.h
SRCS+=  device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
