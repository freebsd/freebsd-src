# Makefile for the act2000 ISDN device driver

# The target object and module list name.

O_TARGET	:= vmlinux-obj.o

# Multipart objects.

list-multi	:= act2000.o
act2000-objs	:= module.o capi.o act2000_isa.o

# Each configuration option enables a list of files.

obj-$(CONFIG_ISDN_DRV_ACT2000)	+= act2000.o

include $(TOPDIR)/Rules.make

# Link rules for multi-part drivers.

act2000.o: $(act2000-objs)
	$(LD) -r -o $@ $(act2000-objs)
