#
# ia64/platform/dig/Makefile
#
# Copyright (C) 1999 Silicon Graphics, Inc.
# Copyright (C) Srinivasa Thirumalachar (sprasad@engr.sgi.com)
#

.S.s:
	$(CPP) $(AFLAGS) -o $*.s $<
.S.o:
	$(CC) $(AFLAGS) -c -o $*.o $<

all: dig.a

O_TARGET := dig.a

obj-y := setup.o
obj-$(CONFIG_IA64_GENERIC) += machvec.o

clean::

include $(TOPDIR)/Rules.make
