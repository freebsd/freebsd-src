# $FreeBSD$

# Makefile for the SBP-II (Serial Bus Protocol 2/SCSI over IEEE1394)

.PATH: ${.CURDIR}/../../../dev/firewire

CFLAGS+= -g

KMOD	= if_fwe
SRCS	= bus_if.h device_if.h\
	  opt_bus.h opt_firewire.h opt_inet.h\
	  if_fwe.c if_fwevar.h\
	  firewire.h firewirereg.h

opt_fwe.h:
	echo "#define FIREWIRE_ETHEREMU_FREEBSD 1" > opt_fwe.h

.include <bsd.kmod.mk>

