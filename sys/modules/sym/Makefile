# $FreeBSD$

.PATH:	${.CURDIR}/../../dev/sym
KMOD	= sym
SRCS	= sym_hipd.c opt_sym.h opt_cam.h opt_scsi.h
SRCS   += device_if.h bus_if.h pci_if.h
MFILES	= kern/device_if.m kern/bus_if.m pci/pci_if.m

.include <bsd.kmod.mk>
