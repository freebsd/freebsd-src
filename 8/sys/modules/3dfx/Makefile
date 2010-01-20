# $FreeBSD$

MAINTAINER=	cokane@FreeBSD.org

.PATH: $(.CURDIR)/../../dev/tdfx

KMOD=	3dfx
SRCS=	bus_if.h pci_if.h device_if.h tdfx_pci.h tdfx_io.h\
	tdfx_vars.h tdfx_pci.c
INCSRC=	../../sys

# Uncomment this for debugging messages
#CFLAGS+=	-DDEBUG

.include <bsd.kmod.mk>
