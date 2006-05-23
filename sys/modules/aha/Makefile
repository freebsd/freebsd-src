# $FreeBSD: src/sys/modules/aha/Makefile,v 1.8 2001/02/04 16:45:39 imp Exp $

.PATH: ${.CURDIR}/../../dev/aha

KMOD=	aha
SRCS=	aha.c aha_isa.c ahareg.h opt_cam.h device_if.h bus_if.h \
	opt_scsi.h isa_if.h

opt_scsi.h:
	echo "#define SCSI_DELAY 15000" > opt_scsi.h

.include <bsd.kmod.mk>
