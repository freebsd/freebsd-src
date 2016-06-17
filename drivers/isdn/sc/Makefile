# Makefile for the sc ISDN device driver

# The target object and module list name.

O_TARGET	:= vmlinux-obj.o

# Multipart objects.

list-multi	:= sc.o
sc-objs		:= shmem.o init.o debug.o packet.o command.o event.o \
		   ioctl.o interrupt.o message.o timer.o	

# Each configuration option enables a list of files.

obj-$(CONFIG_ISDN_DRV_SC)	+= sc.o

include $(TOPDIR)/Rules.make

# Link rules for multi-part drivers.

sc.o: $(sc-objs)
	$(LD) -r -o $@ $(sc-objs)
