# $FreeBSD: src/sys/modules/firewire/sbp_targ/Makefile,v 1.1.2.1 2003/11/14 12:06:35 simokawa Exp $

# Makefile for the SBP-II Target mode

.PATH: ${.CURDIR}/../../../dev/firewire

KMOD	= sbp_targ
SRCS	= bus_if.h device_if.h \
	  opt_cam.h opt_scsi.h \
	  sbp_targ.c sbp.h \
	  firewire.h firewirereg.h \
	  iec13213.h
KMODDEPS = firewire

.include <bsd.kmod.mk>

