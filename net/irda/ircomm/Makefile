#
# Makefile for the Linux IrDA IrCOMM protocol layer.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now in the main makefile...

O_TARGET := ircomm_and_tty.o

list-multi := ircomm.o ircomm-tty.o
ircomm-objs := ircomm_core.o ircomm_event.o ircomm_lmp.o ircomm_ttp.o
ircomm-tty-objs := ircomm_tty.o ircomm_tty_attach.o ircomm_tty_ioctl.o ircomm_param.o

obj-$(CONFIG_IRCOMM) += ircomm.o ircomm-tty.o

include $(TOPDIR)/Rules.make

ircomm.o:  $(ircomm-objs)
	$(LD) $(LD_RFLAG) -r -o $@ $(ircomm-objs)

ircomm-tty.o: $(ircomm-tty-objs)
	$(LD) $(LD_RFLAG) -r -o $@ $(ircomm-tty-objs)

