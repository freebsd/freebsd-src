#       $FreeBSD$ 

MAINTAINER	 = n_hibma@freebsd.org
S		 = ${.CURDIR}/../..

.PATH:  	 $S/dev/usb
KMOD		 = umodem
SRCS		 = bus_if.h device_if.h vnode_if.h opt_usb.h umodem.c
NOMAN		 =
CLEANFILES	+= bus_if.h device_if.h vnode_if.h opt_usb.h \
		   vnode_if.c

opt_usb.h:
	echo > opt_usb.h

bus_if.h:
	perl $S/kern/makedevops.pl -h $S/kern/bus_if.m
device_if.h:
	perl $S/kern/makedevops.pl -h $S/kern/device_if.m

.include <bsd.kmod.mk>
