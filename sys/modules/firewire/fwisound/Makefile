# Makefile for the Apple FireWire audio driver

.PATH: ${SRCTOP}/sys/dev/firewire

KMOD	= fwisound
SRCS	= bus_if.h device_if.h \
	  channel_if.h mixer_if.h feeder_if.h \
	  fwisound.c fwisound_pcm.c fwisound.h fw_helpers.h \
	  firewire.h firewirereg.h \
	  iec13213.h

.include <bsd.kmod.mk>
