# $FreeBSD$

.PATH:	${.CURDIR}/../../dev/fb
KMOD		= s3
SRCS		= device_if.h bus_if.h isa_if.h pci_if.h
SRCS		+= s3pci.c
KMODDEPS 	= vesa

.include <bsd.kmod.mk>
