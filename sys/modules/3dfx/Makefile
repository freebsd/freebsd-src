# $FreeBSD: src/sys/modules/3dfx/Makefile,v 1.11.14.2.8.1 2008/10/02 02:57:24 kensmith Exp $

MAINTAINER=	cokane@FreeBSD.org

.PATH: $(.CURDIR)/../../dev/tdfx

KMOD=	3dfx
SRCS=	bus_if.h pci_if.h device_if.h tdfx_pci.h tdfx_io.h\
	tdfx_vars.h tdfx_pci.c
INCSRC=	../../sys

# Uncomment this for debugging messages
#CFLAGS+=	-DDEBUG

.include <bsd.kmod.mk>
