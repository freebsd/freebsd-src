# $FreeBSD: src/sys/modules/firewire/sbp_targ/Makefile,v 1.1 2003/10/18 05:41:31 simokawa Exp $

# Makefile for the SBP-II Target mode

.PATH: ${.CURDIR}/../../../dev/firewire

KMOD	= sbp_targ
SRCS	= bus_if.h device_if.h \
	  opt_cam.h opt_scsi.h \
	  sbp_targ.c sbp.h \
	  firewire.h firewirereg.h \
	  iec13213.h

#CFLAGS += -g
.include <bsd.kmod.mk>

