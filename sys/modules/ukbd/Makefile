#       $FreeBSD: src/sys/modules/ukbd/Makefile,v 1.10 2000/01/20 13:25:18 yokota Exp $ 

MAINTAINER	 = n_hibma@freebsd.org

.PATH:  	 ${.CURDIR}/../../dev/usb
KMOD		 = ukbd
SRCS		 = bus_if.h device_if.h vnode_if.h \
		   opt_usb.h opt_kbd.h opt_ukbd.h \
		   ukbd.c
NOMAN		 =

opt_kbd.h:
	echo '#define KBD_INSTALL_CDEV 1' > opt_kbd.h

.include <bsd.kmod.mk>
