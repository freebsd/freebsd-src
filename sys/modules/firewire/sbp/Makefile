# $FreeBSD: src/sys/modules/firewire/sbp/Makefile,v 1.2 2002/10/14 19:18:30 jhb Exp $

# Makefile for the SBP-II (Serial Bus Protocol 2/SCSI over IEEE1394)

.PATH: ${.CURDIR}/../../../dev/firewire

KMOD	= sbp
SRCS	= bus_if.h device_if.h \
	  opt_cam.h opt_scsi.h \
	  sbp.c \
	  firewire.h firewirereg.h \
	  iec13213.h

.include <bsd.kmod.mk>

