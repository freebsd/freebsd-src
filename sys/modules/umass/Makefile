#       $FreeBSD$ 

MAINTAINER	 = n_hibma@freebsd.org
S		 = ${.CURDIR}/../..

.PATH:  	 $S/dev/usb
KMOD		 = umass
SRCS		 = bus_if.h device_if.h \
		   opt_usb.h opt_cam.h opt_scsi.h \
		   umass.c
NOMAN		 =
CLEANFILES	+= bus_if.h device_if.h \
		   opt_usb.h opt_cam.h opt_scsi.h

opt_usb.h:
	echo > opt_usb.h
opt_scsi.h:
	echo '#define SCSI_DELAY 15000' > opt_scsi.h
opt_cam.h:
	echo > opt_cam.h

bus_if.h:
	perl $S/kern/makedevops.pl -h $S/kern/bus_if.m
device_if.h:
	perl $S/kern/makedevops.pl -h $S/kern/device_if.m

.include <bsd.kmod.mk>
