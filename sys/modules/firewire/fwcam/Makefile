# Makefile for the IIDC 1394-based Digital Camera driver

.PATH: ${SRCTOP}/sys/dev/firewire

KMOD	= fwcam
SRCS	= bus_if.h device_if.h \
	  fwcam.c fwcam.h fw_helpers.h \
	  firewire.h firewirereg.h \
	  iec13213.h

.include <bsd.kmod.mk>
