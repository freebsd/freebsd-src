#       $FreeBSD$ 

MAINTAINER	 = n_hibma@freebsd.org
SOURCE		 = ${.CURDIR}/../../dev/usb

.PATH:  ${SOURCE}
KMOD		 = umass_mod
SRCS		 = bus_if.h device_if.h vnode_if.h \
		   opt_usb.h opt_cam.h opt_scsi.h \
		   umass.c
NOMAN		 =
#CFLAGS		+= -g
CFLAGS		= -g
CLEANFILES	+= bus_if.h device_if.h vnode_if.h \
		   opt_usb.h opt_cam.h opt_scsi.h \
		   vnode_if.c

opt_usb.h:
	echo > opt_usb.h
#	echo '#define UMASS_DEBUG 1' >> opt_usb.h
opt_scsi.h:
	echo '#define SCSI_DELAY 15000' > opt_scsi.h
opt_cam.h:
	echo > opt_cam.h

bus_if.h:
	perl ${KERN}/makedevops.pl -h ${KERN}/bus_if.m
device_if.h:
	perl ${KERN}/makedevops.pl -h ${KERN}/device_if.m

.include <bsd.kmod.mk>
