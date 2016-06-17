
O_TARGET := atyfb.o

export-objs    :=  atyfb_base.o mach64_accel.o

obj-y				:= atyfb_base.o mach64_accel.o
obj-$(CONFIG_FB_ATY_GX)		+= mach64_gx.o
obj-$(CONFIG_FB_ATY_CT)		+= mach64_ct.o mach64_cursor.o
obj-m				:= $(O_TARGET)

include $(TOPDIR)/Rules.make

