# $FreeBSD: src/sys/modules/agp/Makefile,v 1.2.2.3 2000/07/19 12:35:41 ru Exp $

.PATH:	${.CURDIR}/../../pci
KMOD	= agp
SRCS	= agp.c agp_intel.c agp_via.c agp_sis.c agp_ali.c agp_amd.c
SRCS   += agp_i810.c agp_if.c
SRCS   += device_if.h bus_if.h agp_if.h pci_if.h
SRCS   += opt_bdg.h opt_bus.h opt_pci.h opt_smp.h
MFILES	= kern/device_if.m kern/bus_if.m pci/agp_if.m pci/pci_if.m

.include <bsd.kmod.mk>
