#
# Makefile for the agpgart device driver.  This driver adds a user
# space ioctl interface to use agp memory.  It also adds a kernel interface
# that other drivers could use to manipulate agp memory.

O_TARGET	:= agp.o

export-objs := agpgart_be.o

list-multi := agpgart.o
agpgart-objs := agpgart_fe.o agpgart_be.o

obj-$(CONFIG_AGP) += agpgart.o

include $(TOPDIR)/Rules.make

agpgart.o: $(agpgart-objs)
	$(LD) $(LD_RFLAG) -r -o $@ $(agpgart-objs)
