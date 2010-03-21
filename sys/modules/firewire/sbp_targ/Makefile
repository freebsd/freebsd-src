# $FreeBSD: src/sys/modules/firewire/sbp_targ/Makefile,v 1.3.12.1 2010/02/10 00:26:20 kensmith Exp $

# Makefile for the SBP-II Target mode

.PATH: ${.CURDIR}/../../../dev/firewire

KMOD	= sbp_targ
SRCS	= bus_if.h device_if.h \
	  opt_cam.h opt_scsi.h \
	  sbp_targ.c sbp.h \
	  firewire.h firewirereg.h \
	  iec13213.h

.include <bsd.kmod.mk>
