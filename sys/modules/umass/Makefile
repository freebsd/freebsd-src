#       $FreeBSD: src/sys/modules/umass/Makefile,v 1.7 1999/11/28 18:53:33 bde Exp $ 

MAINTAINER	 = n_hibma@freebsd.org

.PATH:  	 ${.CURDIR}/../../dev/usb
KMOD		 = umass
SRCS		 = bus_if.h device_if.h \
		   opt_usb.h opt_cam.h opt_scsi.h \
		   umass.c
NOMAN		 =

opt_scsi.h:
	echo '#define SCSI_DELAY 15000' > opt_scsi.h

.include <bsd.kmod.mk>
