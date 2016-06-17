#
# ia64/platform/hp/sim/Makefile
#
# Copyright (C) 2002 Hewlett-Packard Co.
#	David Mosberger-Tang <davidm@hpl.hp.com>
# Copyright (C) 1999 Silicon Graphics, Inc.
# Copyright (C) Srinivasa Thirumalachar (sprasad@engr.sgi.com)
#

O_TARGET := sim.o

obj-y := hpsim_console.o hpsim_irq.o hpsim_setup.o
obj-$(CONFIG_IA64_GENERIC) += hpsim_machvec.o

include $(TOPDIR)/Rules.make
