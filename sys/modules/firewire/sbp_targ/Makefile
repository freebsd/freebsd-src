# $FreeBSD: src/sys/modules/firewire/sbp_targ/Makefile,v 1.3.10.1.8.1 2012/03/03 06:15:13 kensmith Exp $

# Makefile for the SBP-II Target mode

.PATH: ${.CURDIR}/../../../dev/firewire

KMOD	= sbp_targ
SRCS	= bus_if.h device_if.h \
	  opt_cam.h opt_scsi.h \
	  sbp_targ.c sbp.h \
	  firewire.h firewirereg.h \
	  iec13213.h

.include <bsd.kmod.mk>
