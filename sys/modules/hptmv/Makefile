#
# Makefile for RR182x FreeBSD driver
# Copyright (c)  2003-2004 HighPoint Technologies, Inc. All rights reserved
#
# $FreeBSD$

HPTMV= ${.CURDIR}/../../contrib/dev/hptmv
.PATH: ${.CURDIR}/../../dev/hptmv ${HPTMV}

KMOD= hptmv
SRCS= opt_scsi.h opt_cam.h
SRCS+= bus_if.h device_if.h pci_if.h
SRCS+= mv.c entry.c vers.c
SRCS+= ioctl.c hptproc.c gui_lib.c
OBJS+= raid.o

raid.o:	${HPTMV}/i386-elf.raid.o.uu
	uudecode -p < ${HPTMV}/i386-elf.raid.o.uu > ${.TARGET}

#
# Driver version number.
#
.ifndef DRIVER_VERSION
DRIVER_VERSION = "1.1"
.endif

vers.c: mv.o
	@echo char DRIVER_VERSION[]=\"$(DRIVER_VERSION)\"\; > ${.OBJDIR}/$(.TARGET)

# calculate system pointer size.
BITS_PER_LONG= 4

#
# Debug Options:
#
#   -DDEBUG: include debug code
#   -DDEBUG_LEVEL=x:
#		0 - No debug output message
#		1 - print only error messages
#		2 - print error and warning
#		3 - print all messages (info, warning, error)
#
#DEBUGOPT = -O2

.if defined(DEBUG) && $(DEBUG) == 1
DEBUGOPT += -DDEBUG
.if defined(DEBUG_LEVEL)
DEBUGOPT += -DDEBUG_LEVEL=$(DEBUG_LEVEL)
.else
DEBUGOPT += -DDEBUG_LEVEL=1
.endif
.endif

.if defined(FOR_DEMO) && $(FOR_DEMO) == 1
DEBUGOPT += -DFOR_DEMO
.endif

CFLAGS+= -D__KERNEL__=1 -DKERNEL=1 -D_KERNEL=1 -D_FREEBSD_=1 ${DEBUGOPT} \
         -DBITS_PER_LONG=$(BITS_PER_LONG)

CLEANFILES += vers.c
.include <bsd.kmod.mk>
