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

S	= ${.CURDIR}/../..
.PATH:  $S/dev/bktr
KMOD	= bktr 
SRCS	= bktr_core.c bktr_os bktr_audio bktr_tuner bktr_card bktr.h opt_devfs.h opt_bktr.h smbus.h bus_if.h device_if.h pci_if.h vnode_if.h
CLEANFILES	+= bktr.h opt_devfs.h opt_bktr.h smbus.h bus_if.h device_if.h pci_if.h vnode_if.h
FLAGS		+= ${DEBUG_FLAGS}

bktr.h:
	echo "#define NBKTR 1" > bktr.h

smbus.h:
	# Does not use SMBUS/IICBUS. Uses the old i2c code self contained in
	# the bt848 driver.
	echo "#define NSMBUS 0" > smbus.h

opt_devfs.h:
	touch opt_devfs.h

opt_bktr.h:
	touch opt_bktr.h

device_if.h: $S/kern/makedevops.pl $S/kern/device_if.m
	perl $S/kern/makedevops.pl -h $S/kern/device_if.m

bus_if.h: $S/kern/makedevops.pl $S/kern/bus_if.m
	perl $S/kern/makedevops.pl -h $S/kern/bus_if.m

pci_if.h: $S/kern/makedevops.pl $S/pci/pci_if.m
	perl $S/kern/makedevops.pl -h $S/pci/pci_if.m

.include <bsd.kmod.mk>
