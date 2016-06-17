#
# Makefile for the ATM Protocol Families.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now in the main makefile...

O_TARGET	:= atm.o

export-objs 	:= common.o atm_misc.o raw.o resources.o ipcommon.o proc.o

list-multi	:= mpoa.o
mpoa-objs	:= mpc.o mpoa_caches.o mpoa_proc.o

obj-y		:= addr.o pvc.o signaling.o svc.o common.o atm_misc.o raw.o resources.o
ifeq ($(CONFIG_ATM),m)
  obj-m += $(O_TARGET)
endif

ifneq ($(CONFIG_ATM_CLIP),n)
  NEED_IPCOM = ipcommon.o
endif
obj-$(CONFIG_ATM_CLIP) += clip.o

ifeq ($(CONFIG_ATM_BR2684),y)
  NEED_IPCOM = ipcommon.o
else
  ifeq ($(CONFIG_ATM_BR2684),m)
	NEED_IPCOM = ipcommon.o
  endif
endif
obj-$(CONFIG_ATM_BR2684) += br2684.o

ifeq ($(CONFIG_NET_SCH_ATM),y)
  NEED_IPCOM = ipcommon.o
endif

obj-y += $(NEED_IPCOM)

ifeq ($(CONFIG_PROC_FS),y)
  obj-y += proc.o
endif

obj-$(CONFIG_ATM_LANE) += lec.o
obj-$(CONFIG_ATM_MPOA) += mpoa.o
obj-$(CONFIG_PPPOATM) += pppoatm.o

include $(TOPDIR)/Rules.make

mpoa.o: $(mpoa-objs)
	$(LD) -r -o mpoa.o $(mpoa-objs)
