#
# S/390 character devices
#

O_TARGET := s390-char.o

list-multi	:= tub3270.o tape390.o
export-objs	:= hwc_rw.o

tub3270-objs := tuball.o tubfs.o tubtty.o \
                     tubttyaid.o tubttybld.o tubttyscl.o \
                     tubttyrcl.o tubttysiz.o

tape390-$(CONFIG_S390_TAPE_CHAR) += tapechar.o
tape390-$(CONFIG_S390_TAPE_BLOCK) += tapeblock.o
tape390-$(CONFIG_S390_TAPE_3480) += tape3480.o tape34xx.o
tape390-$(CONFIG_S390_TAPE_3490) += tape3490.o tape34xx.o
tape390-objs := tape.o $(sort $(tape390-y))

obj-y += ctrlchar.o
obj-$(CONFIG_TN3215) += con3215.o
obj-$(CONFIG_HWC) += hwc_con.o hwc_rw.o hwc_tty.o
obj-$(CONFIG_HWC_CPI) += hwc_cpi.o
obj-$(CONFIG_TN3270) += tub3270.o
obj-$(CONFIG_S390_TAPE) += tape390.o

include $(TOPDIR)/Rules.make

tub3270.o: $(tub3270-objs)
	$(LD) -r -o $@ $(tub3270-objs)

tape390.o: $(tape390-objs)
	$(LD) -r -o $@ $(tape390-objs)

