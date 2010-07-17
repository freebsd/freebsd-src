# $FreeBSD: src/sys/modules/iir/Makefile,v 1.1.38.1.4.1 2010/06/14 02:09:06 kensmith Exp $

KMOD		=  iir
.PATH:		${.CURDIR}/../../dev/${KMOD}
SRCS		=  iir.c iir_ctrl.c iir_pci.c
SRCS		+= opt_scsi.h opt_cam.h
SRCS		+= device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
