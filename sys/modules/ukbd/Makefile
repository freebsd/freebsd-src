#       $FreeBSD$ 

MAINTAINER	 = n_hibma@freebsd.org

.PATH:  	 ${.CURDIR}/../../dev/usb
KMOD		 = ukbd
SRCS		 = bus_if.h device_if.h vnode_if.h \
		   opt_usb.h ukbd.h opt_kbd.h opt_ukbd.h \
		   ukbd.c
NOMAN		 =
CLEANFILES	 = ukbd.h opt_kbd.h opt_ukbd.h

ukbd.h:
	echo > ukbd.h

opt_kbd.h:
	echo '#define KBD_INSTALL_CDEV 1' > opt_kbd.h

opt_ukbd.h:
	echo > opt_ukbd.h

.include <bsd.kmod.mk>
