# $FreeBSD$
# $Whistle: Makefile,v 1.2 1999/01/19 19:39:22 archie Exp $

KMOD=		ng_tty
SRCS= 		ng_tty.c device_if.h bus_if.h pci_if.h
MFILES=		kern/device_if.m kern/bus_if.m pci/agp_if.m pci/pci_if.m
NOMAN=

.include <bsd.kmod.mk>
