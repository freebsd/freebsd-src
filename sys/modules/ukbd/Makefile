#       $FreeBSD$ 

MAINTAINER	 = n_hibma@freebsd.org
SOURCE		 = ${.CURDIR}/../../dev/usb

.PATH:  ${SOURCE}
KMOD		 = ukbd_mod
SRCS		 = bus_if.h device_if.h vnode_if.h \
		   opt_usb.h ukbd.h opt_kbd.h \
		   ukbd.c
NOMAN		 =
CFLAGS		+= -g
CLEANFILES	+= bus_if.h device_if.h vnode_if.h \
		   opt_usb.h ukbd.h opt_kbd.h \
		   vnode_if.c

opt_usb.h:
	echo > opt_usb.h
ukbd.h:
	echo > ukbd.h
opt_kbd.h:
	echo '#define KBD_INSTALL_CDEV 1' > opt_kbd.h

bus_if.h:
	perl ${KERN}/makedevops.pl -h ${KERN}/bus_if.m
device_if.h:
	perl ${KERN}/makedevops.pl -h ${KERN}/device_if.m

.include <bsd.kmod.mk>
