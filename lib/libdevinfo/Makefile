# $FreeBSD$

PACKAGE=	devmatch
LIB=	devinfo
SRCS=	devinfo.c
INCS=	devinfo.h
MAN=	devinfo.3

MLINKS+=devinfo.3 devinfo_init.3
MLINKS+=devinfo.3 devinfo_free.3
MLINKS+=devinfo.3 devinfo_handle_to_device.3
MLINKS+=devinfo.3 devinfo_handle_to_resource.3
MLINKS+=devinfo.3 devinfo_handle_to_rman.3
MLINKS+=devinfo.3 devinfo_foreach_device_child.3
MLINKS+=devinfo.3 devinfo_foreach_device_resource.3
MLINKS+=devinfo.3 devinfo_foreach_rman_resource.3
MLINKS+=devinfo.3 devinfo_foreach_rman.3

SHLIB_MAJOR=	6

WARNS?=	3

.include <bsd.lib.mk>
