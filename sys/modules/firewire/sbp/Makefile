# $FreeBSD$

# Makefile for the SBP-II (Serial Bus Protocol 2/SCSI over IEEE1394)

.PATH: ${.CURDIR}/../../../dev/firewire

CFLAGS+= -g

KMOD	= sbp
SRCS	= bus_if.h device_if.h\
	  opt_bus.h opt_firewire.h opt_cam.h opt_scsi.h\
	  sbp.c \
	  firewire.h firewirereg.h \
	  iec13213.h

opt_sbp.h:
	echo "#define SBP2_FREEBSD_MODULE 1" > opt_sbp.h

.include <bsd.kmod.mk>

