# Makefile for tws (LSI 3ware 9750 SAS2/SATA-II RAID PCIe) driver
# $FreeBSD: src/sys/modules/tws/Makefile,v 1.1.4.2.2.1 2012/03/03 06:15:13 kensmith Exp $

KMOD=	tws
.PATH:	${.CURDIR}/../../dev/${KMOD}
 
SRCS=	tws.c tws_services.c tws_cam.c tws_hdm.c tws_user.c
SRCS+=	device_if.h bus_if.h pci_if.h opt_cam.h opt_scsi.h

.include <bsd.kmod.mk>
