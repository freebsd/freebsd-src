# Makefile for the AV/C DV capture driver

.PATH: ${SRCTOP}/sys/dev/firewire

KMOD	= fwdv
SRCS	= bus_if.h device_if.h \
	  fwdv.c fwdv.h fw_helpers.h \
	  firewire.h firewirereg.h \
	  iec13213.h iec68113.h

.include <bsd.kmod.mk>
