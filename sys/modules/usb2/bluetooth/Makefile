#
# $FreeBSD$
#

S=     ${.CURDIR}/../../..

.PATH: $S/dev/usb2/bluetooth

KMOD=  usb2_bluetooth
SRCS= 
SRCS+= bus_if.h usb2_if.h device_if.h vnode_if.h
SRCS+= opt_usb.h opt_bus.h opt_netgraph.h
SRCS+= usb2_bluetooth.c

.include <bsd.kmod.mk>
