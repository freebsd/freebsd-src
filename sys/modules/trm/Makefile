# $FreeBSD: src/sys/modules/trm/Makefile,v 1.1 2002/10/13 18:44:26 cognet Exp $

.PATH: ${.CURDIR}/../../dev/trm

KMOD=	trm
SRCS=	trm.c trm.h opt_cam.h device_if.h bus_if.h \
	opt_scsi.h pci_if.h

opt_scsi.h:
	echo "#define SCSI_DELAY 15000" > opt_scsi.h

.include <bsd.kmod.mk>
