#
# Makefile for the LSI Logic Fusion MPT (Message Passing Technology) drivers.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now inherited from the
# parent makefile.
#
# Note 3! If you want to turn on various debug defines for an extended period of
# time but don't want them lingering around in the Makefile when you pass it on
# to someone else, use the MPT_CFLAGS env variable (thanks Steve). -nromer

#=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-{ LSI_LOGIC

#  Architecture-specific...
#			# intel
#EXTRA_CFLAGS += -g
#			# sparc64
#EXTRA_CFLAGS += -gstabs+

EXTRA_CFLAGS += -I. ${MPT_CFLAGS}

# Fusion MPT drivers; recognized debug defines...
#  MPT general:
#EXTRA_CFLAGS += -DDEBUG
#EXTRA_CFLAGS += -DMPT_DEBUG
#EXTRA_CFLAGS += -DMPT_DEBUG_MSG_FRAME
#EXTRA_CFLAGS += -DMPT_DEBUG_SG
#EXTRA_CFLAGS += -DMPTSCSIH_DBG_TIMEOUT
#
# driver/module specifics...
#
#  For mptbase:
#CFLAGS_mptbase.o += -DMPT_DEBUG_HANDSHAKE
#CFLAGS_mptbase.o += -DMPT_DEBUG_IRQ
#
#  For mptscsih:
#CFLAGS_mptscsih.o += -DMPT_DEBUG_DV
#CFLAGS_mptscsih.o += -DMPT_DEBUG_DV_TINY
#CFLAGS_mptscsih.o += -DMPT_DEBUG_RESET
#CFLAGS_mptscsih.o += -DMPT_DEBUG_NEH
#
#  For mptctl:
#CFLAGS_mptctl.o += -DMPT_DEBUG_IOCTL
#
#  For mptlan:
#CFLAGS_mptlan.o += -DMPT_LAN_IO_DEBUG
#
#  For isense:

#  EXP...
##mptscsih-objs	:= scsihost.o scsiherr.o

#=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-} LSI_LOGIC

O_TARGET := fusion.o

export-objs	:= mptbase.o mptscsih.o mptlan.o mptctl.o isense.o

# ? what's list-multi for?
#list-multi	:= fusion.o mptscsih.o

obj-$(CONFIG_FUSION)		+= mptbase.o mptscsih.o
obj-$(CONFIG_FUSION_ISENSE)	+= isense.o
obj-$(CONFIG_FUSION_CTL)	+= mptctl.o
obj-$(CONFIG_FUSION_LAN)	+= mptlan.o

O_OBJS		:= $(filter-out		$(export-objs), $(obj-y))
OX_OBJS		:= $(filter		$(export-objs), $(obj-y))
M_OBJS		:= $(sort $(filter-out	$(export-objs), $(obj-m)))
MX_OBJS		:= $(sort $(filter	$(export-objs), $(obj-m)))

include $(TOPDIR)/Rules.make


#  EXP...
## Fusion MPT extra's...
##mptscsih.o: $(mptscsih-objs)
##	$(LD) -r -o $@ $(mptscsih-objs)
