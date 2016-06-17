# Makefile for the kernel ISDN subsystem and device drivers.

# The target object and module list name.

O_TARGET	:= vmlinux-obj.o

# Objects that export symbols.

export-objs	:= isdn_common.o

# Multipart objects.

list-multi	:= isdn.o
isdn-objs	:= isdn_net.o isdn_tty.o isdn_v110.o isdn_common.o

# Optional parts of multipart objects.

isdn-objs-$(CONFIG_ISDN_PPP)		+= isdn_ppp.o
isdn-objs-$(CONFIG_ISDN_X25)		+= isdn_concap.o isdn_x25iface.o
isdn-objs-$(CONFIG_ISDN_AUDIO)		+= isdn_audio.o
isdn-objs-$(CONFIG_ISDN_TTY_FAX)	+= isdn_ttyfax.o
isdn-objs-$(CONFIG_ISDN_WITH_ABC)	+= isdn_dwabc.o

isdn-objs				+= $(isdn-objs-y)

# Ordering constraints: isdn.o first, rest doesn't matter

# Each configuration option enables a list of files.

obj-$(CONFIG_ISDN)			+= isdn.o
obj-$(CONFIG_ISDN_PPP_BSDCOMP)		+= isdn_bsdcomp.o

# Object files in subdirectories

mod-subdirs				:= avmb1 eicon hisax
subdir-$(CONFIG_ISDN_DIVERSION)		+= divert
subdir-$(CONFIG_ISDN_HISAX)		+= hisax
subdir-$(CONFIG_ISDN_DRV_ICN)		+= icn
subdir-$(CONFIG_ISDN_DRV_PCBIT)		+= pcbit
subdir-$(CONFIG_ISDN_DRV_SC)		+= sc
subdir-$(CONFIG_ISDN_CAPI)		+= avmb1
subdir-$(CONFIG_ISDN_DRV_LOOP)		+= isdnloop
subdir-$(CONFIG_ISDN_DRV_ACT2000)	+= act2000
subdir-$(CONFIG_ISDN_DRV_EICON)		+= eicon
subdir-$(CONFIG_HYSDN)			+= hysdn
subdir-$(CONFIG_ISDN_DRV_TPAM)		+= tpam

obj-y += $(addsuffix /vmlinux-obj.o, $(subdir-y))

# The global Rules.make.

include $(TOPDIR)/Rules.make

# Link rules for multi-part drivers.

isdn.o: $(isdn-objs)
	$(LD) -r -o $@ $(isdn-objs)
