# $FreeBSD: src/sys/modules/iir/Makefile,v 1.1.38.1.6.1 2010/12/21 17:09:25 kensmith Exp $

KMOD		=  iir
.PATH:		${.CURDIR}/../../dev/${KMOD}
SRCS		=  iir.c iir_ctrl.c iir_pci.c
SRCS		+= opt_scsi.h opt_cam.h
SRCS		+= device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
