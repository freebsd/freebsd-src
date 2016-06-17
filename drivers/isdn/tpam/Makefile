# Makefile for the TurboPAM ISDN device driver

# The target object and module list name.

O_TARGET	:= vmlinux-obj.o

# Multipart objects.

list-multi	:= tpam.o
tpam-objs	:= tpam_main.o tpam_nco.o tpam_memory.o tpam_commands.o tpam_queues.o tpam_hdlc.o tpam_crcpc.o

# Each configuration option enables a list of files.

obj-$(CONFIG_ISDN_DRV_TPAM)	+= tpam.o

include $(TOPDIR)/Rules.make

# Link rules for multi-part drivers.

tpam.o: $(tpam-objs)
	$(LD) -r -o $@ $(tpam-objs)
