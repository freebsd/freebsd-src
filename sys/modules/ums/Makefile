#       $FreeBSD$ 

MAINTAINER	 = n_hibma@freebsd.org
SOURCE		 = ${.CURDIR}/../../dev/usb

.PATH:  ${SOURCE}
KMOD		 = ums
SRCS		 = bus_if.h device_if.h vnode_if.h opt_usb.h ums.c
NOMAN		 =
CFLAGS		+= -g
CLEANFILES	+= bus_if.h device_if.h vnode_if.h opt_usb.h \
		   vnode_if.c

opt_usb.h:
	echo > opt_usb.h

bus_if.h:
	perl ${KERN}/makedevops.pl -h ${KERN}/bus_if.m
device_if.h:
	perl ${KERN}/makedevops.pl -h ${KERN}/device_if.m

.include <bsd.kmod.mk>
