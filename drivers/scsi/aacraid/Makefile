
EXTRA_CFLAGS	+= -I$(TOPDIR)/drivers/scsi

O_TARGET	:= aacraid.o
obj-m		:= $(O_TARGET)

obj-y		:= linit.o aachba.o commctrl.o comminit.o commsup.o \
		   dpcsup.o rx.o sa.o

include $(TOPDIR)/Rules.make
