# $Id: Makefile.def,v 1.14 2006/04/06 09:23:49 gmm Exp $
# $FreeBSD$

RR232X= ${.CURDIR}/../../dev/rr232x
.PATH: ${RR232X}

KMOD = rr232x
SRCS = opt_scsi.h opt_cam.h bus_if.h device_if.h pci_if.h os_bsd.h os_bsd.c osm_bsd.c rr232x_config.c
OBJS = rr232x_lib.o

ARCH ?= $(MACHINE_ARCH)
.if $(MACHINE_ARCH)!=$(ARCH)
HOSTARCH != uname -m
CROSS_COMPILE ?= /usr/obj/$(ARCH)/usr/src/$(HOSTARCH)/usr/bin/
.MAKEFLAGS:	MACHINE_ARCH=$(ARCH) CC=$(CROSS_COMPILE)$(CC) LD=$(CROSS_COMPILE)$(LD)\
		NM=$(CROSS_COMPILE)nm OBJCOPY=$(CROSS_COMPILE)objcopy
.endif

rr232x_lib.o:
	uudecode -p < ${RR232X}/$(MACHINE_ARCH)-elf.rr232x_lib.o.uu > ${.TARGET}

.include <bsd.kmod.mk>
