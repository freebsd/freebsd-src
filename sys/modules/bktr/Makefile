# Makefile for the bktr Brooktree Bt848/Bt878 PCI video capture device
# Using the new FreeBSD 4.x device driver API (newbus) the Bt848 driver
# can be loaded dynamically both in the boot loader phase and once
# the system has booted.
# The driver can also be unloaded, recompiled and reloaded.
#
# Use kldload bktr   and kldunload bktr  to load/unload the driver
#
# Note - as the driver requires contiguous memory, loading the driver once
# memory has filled or become fragmented may fail.

# $FreeBSD$

.PATH:  ${.CURDIR}/../../dev/bktr
KMOD=	bktr 
SRCS=	bktr_core.c bktr_os.c bktr_audio.c bktr_tuner.c bktr_card.c \
	bktr.h opt_bktr.h smbus.h bus_if.h device_if.h \
	pci_if.h vnode_if.h
CLEANFILES= bktr.h smbus.h

bktr.h:
	echo "#define NBKTR 1" > bktr.h

# Does not use SMBUS/IICBUS. Uses the old i2c code self contained in
# the bt848 driver.
smbus.h:
	echo "#define NSMBUS 0" > smbus.h

.include <bsd.kmod.mk>
