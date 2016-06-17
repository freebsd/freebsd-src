# Makefile for the hysdn ISDN device driver

# The target object and module list name.

O_TARGET	:= vmlinux-obj.o

# Multipart objects.

list-multi	:= hysdn.o
hysdn-objs	:= hysdn_procconf.o hysdn_proclog.o boardergo.o hysdn_boot.o \
		   hysdn_sched.o hysdn_net.o hysdn_init.o

# Optional parts of multipart objects.

hysdn-objs-$(CONFIG_HYSDN_CAPI) += hycapi.o

hysdn-objs 	+= $(hysdn-objs-y)

# Each configuration option enables a list of files.

obj-$(CONFIG_HYSDN)	+= hysdn.o

include $(TOPDIR)/Rules.make

# Link rules for multi-part drivers.

hysdn.o: $(hysdn-objs)
	$(LD) -r -o $@ $(hysdn-objs)

