USE_STANDARD_AS_RULE := true

all: sbswarm.a

OBJS-y                   = setup.o cmdline.o rtc_xicor1241.o rtc_m41t81.o

OBJS-$(CONFIG_KGDB) += dbg_io.o

sbswarm.a: $(OBJS-y)
	$(AR) rcs sbswarm.a $^

include $(TOPDIR)/Rules.make

