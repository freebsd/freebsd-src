# $FreeBSD: src/sys/modules/3dfx/Makefile,v 1.13.10.1.6.1 2010/12/21 17:09:25 kensmith Exp $

MAINTAINER=	cokane@FreeBSD.org

.PATH: $(.CURDIR)/../../dev/tdfx

KMOD=	3dfx
SRCS=	bus_if.h pci_if.h device_if.h tdfx_pci.h tdfx_io.h\
	tdfx_vars.h tdfx_pci.c
INCSRC=	../../sys

# Uncomment this for debugging messages
#CFLAGS+=	-DDEBUG

.include <bsd.kmod.mk>
