#
# $FreeBSD$
#
# Makefile for the FreeBSD specific LibUSB 2.0
#

LIB=		usb
SHLIB_MAJOR=	2
SHLIB_MINOR=	0
SRCS=		libusb20.c
SRCS+=		libusb20_desc.c
SRCS+=		libusb20_ugen20.c
INCS+=		libusb20.h
INCS+=		libusb20_desc.h
MAN=		libusb.3 libusb20.3
MKLINT=		no
NOGCCERROR=

WARNS?=		2

MLINKS+=	libusb.3 usb.3

# libusb 0.1 compat
INCS+=		usb.h
SRCS+=		libusb20_compat01.c

# libusb 1.0 compat
INCS+=		libusb.h
SRCS+=		libusb10.c
SRCS+=		libusb10_desc.c
SRCS+=		libusb10_io.c

.include <bsd.lib.mk>

