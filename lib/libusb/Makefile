#
# $FreeBSD$
#
# Makefile for the FreeBSD specific LibUSB 2.0
#

LIB=		usb
SHLIB_MAJOR=	1
SHLIB_MINOR=	0
SRCS=		libusb20.c
SRCS+=		libusb20_desc.c
SRCS+=		libusb20_ugen20.c
SRCS+=		libusb20_compat01.c
SRCS+=		libusb20_compat10.c
INCS+=		libusb20.h
INCS+=		libusb20_desc.h
MAN=		libusb.3
MKLINT=		no
NOGCCERROR=

MLINKS+=	libusb.3 usb.3 \
		libusb.3 libusb20.3

# libusb 0.1 compat
INCS+=		usb.h

.include <bsd.lib.mk>

