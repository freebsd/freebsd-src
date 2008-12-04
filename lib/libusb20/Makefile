#
# $FreeBSD$
#
# Makefile for the FreeBSD specific LibUSB 2.0
#

LIB=		usb20
SHLIB_MAJOR=	1
SHLIB_MINOR=	0
SRCS=		libusb20.c
SRCS+=		libusb20_desc.c
SRCS+=		libusb20_ugen20.c
SRCS+=		libusb20_compat01.c
SRCS+=		libusb20_compat10.c
INCS+=		libusb20.h
INCS+=		libusb20_desc.h
INCS+=		libusb20_compat01.h
INCS+=		libusb20_compat10.h
MAN=		libusb20.3
MKLINT=		no
NOGCCERROR=

.include <bsd.lib.mk>

