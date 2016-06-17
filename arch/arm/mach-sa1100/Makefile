#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).

USE_STANDARD_AS_RULE := true

O_TARGET		:= sa1100.o

# Common support (must be linked before board specific support)
obj-y := generic.o irq.o dma-sa1100.o
obj-m :=
obj-n :=
obj-  :=

export-objs :=	assabet.o consus.o badge4.o dma-sa1100.o dma-sa1111.o \
		flexanet.o freebird.o frodo.o generic.o h3600.o \
		huw_webpanel.o irq.o sa1111.o sa1111-pcibuf.o \
		system3.o yopy.o usb_ctl.o usb_recv.o usb_send.o simputer.o ssp.o

# These aren't present yet, and prevents a plain -ac kernel building.
# hwtimer.o

# This needs to be cleaned up.  We probably need to have SA1100
# and SA1110 config symbols.
#
# We link the CPU support next, so that RAM timings can be tuned.
ifeq ($(CONFIG_CPU_FREQ),y)
obj-$(CONFIG_SA1100_ASSABET) += cpu-sa1110.o
obj-$(CONFIG_SA1100_CEP) += cpu-sa1110.o
obj-$(CONFIG_SA1100_CONSUS) += cpu-sa1110.o
obj-$(CONFIG_SA1100_CERF) += cpu-sa1110.o
obj-$(CONFIG_SA1100_HACKKIT) += cpu-sa1110.o
obj-$(CONFIG_SA1100_PT_SYSTEM3) += cpu-sa1110.o
obj-$(CONFIG_SA1100_SIMPUTER) += cpu-sa1110.o
obj-$(CONFIG_SA1100_LART) += cpu-sa1100.o
endif

# Next, the SA1111 stuff.
obj-$(CONFIG_SA1111) += sa1111.o dma-sa1111.o
obj-$(CONFIG_USB_OHCI_SA1111) += sa1111-pcibuf.o

# Specific board support
obj-$(CONFIG_SA1100_ADSAGC) += adsagc.o
obj-$(CONFIG_SA1100_ADSBITSY) += adsbitsy.o
obj-$(CONFIG_SA1100_ADSBITSYPLUS) += adsbitsyplus.o
obj-$(CONFIG_SA1100_ASSABET) += assabet.o
obj-$(CONFIG_ASSABET_NEPONSET) += neponset.o
obj-$(CONFIG_SA1100_BADGE4) +=  badge4.o
obj-$(CONFIG_SA1100_BRUTUS) += brutus.o
obj-$(CONFIG_SA1100_CEP) += cep.o
obj-$(CONFIG_SA1100_CERF) += cerf.o
obj-$(CONFIG_SA1100_CONSUS) += consus.o
obj-$(CONFIG_SA1100_EMPEG) += empeg.o
obj-$(CONFIG_SA1100_FLEXANET) += flexanet.o
obj-$(CONFIG_SA1100_FREEBIRD) += freebird.o
obj-$(CONFIG_SA1100_FRODO) += frodo.o
obj-$(CONFIG_SA1100_GRAPHICSCLIENT) += graphicsclient.o
obj-$(CONFIG_SA1100_GRAPHICSMASTER) += graphicsmaster.o
obj-$(CONFIG_SA1100_H3600) += h3600.o
obj-$(CONFIG_SA1100_HACKKIT) += hackkit.o
obj-$(CONFIG_SA1100_HUW_WEBPANEL) += huw_webpanel.o
obj-$(CONFIG_SA1100_ITSY) += itsy.o
obj-$(CONFIG_SA1100_JORNADA720) += jornada720.o
obj-$(CONFIG_SA1100_LART) += lart.o
obj-$(CONFIG_SA1100_NANOENGINE) += nanoengine.o
obj-$(CONFIG_SA1100_OMNIMETER) += omnimeter.o
obj-$(CONFIG_SA1100_PANGOLIN) += pangolin.o
obj-$(CONFIG_SA1100_PFS168) += pfs168.o
obj-$(CONFIG_SA1100_PLEB) += pleb.o
obj-$(CONFIG_SA1100_PT_SYSTEM3) += system3.o
obj-$(CONFIG_SA1100_SHANNON) += shannon.o
obj-$(CONFIG_SA1100_SHERMAN) += sherman.o
obj-$(CONFIG_SA1100_SIMPAD) += simpad.o
obj-$(CONFIG_SA1100_SIMPUTER) += simputer.o
obj-$(CONFIG_SA1100_VICTOR) += victor.o
obj-$(CONFIG_SA1100_XP860) += xp860.o
obj-$(CONFIG_SA1100_YOPY) += yopy.o

# LEDs support
leds-y := leds.o
leds-$(CONFIG_SA1100_ADSAGC) += leds-adsagc.o
leds-$(CONFIG_SA1100_ADSBITSY) += leds-adsbitsy.o
leds-$(CONFIG_SA1100_ADSBITSYPLUS) += leds-adsbitsyplus.o
leds-$(CONFIG_SA1100_ASSABET) += leds-assabet.o
leds-$(CONFIG_SA1100_BRUTUS) += leds-brutus.o
leds-$(CONFIG_SA1100_CERF) += leds-cerf.o
leds-$(CONFIG_SA1100_CONSUS) += leds-consus.o
leds-$(CONFIG_SA1100_FLEXANET) += leds-flexanet.o
leds-$(CONFIG_SA1100_FRODO) += leds-frodo.o
leds-$(CONFIG_SA1100_GRAPHICSCLIENT) += leds-graphicsclient.o
leds-$(CONFIG_SA1100_GRAPHICSMASTER) += leds-graphicsmaster.o
leds-$(CONFIG_SA1100_HACKKIT) += leds-hackkit.o
leds-$(CONFIG_SA1100_LART) += leds-lart.o
leds-$(CONFIG_SA1100_PFS168) += leds-pfs168.o
leds-$(CONFIG_SA1100_SIMPAD) += leds-simpad.o  
leds-$(CONFIG_SA1100_PT_SYSTEM3) += leds-system3.o
obj-$(CONFIG_LEDS) += $(leds-y)

# SA1110 USB client support
list-multi += sa1100usb_core.o
sa1100usb_core-objs := usb_ctl.o usb_ep0.o usb_recv.o usb_send.o
obj-$(CONFIG_SA1100_USB) += sa1100usb_core.o
obj-$(CONFIG_SA1100_USB_NETLINK) += usb-eth.o
obj-$(CONFIG_SA1100_USB_CHAR) += usb-char.o

# Miscelaneous functions
obj-$(CONFIG_PM) += pm.o sleep.o

obj-$(CONFIG_SA1100_SSP)		+= ssp.o

include $(TOPDIR)/Rules.make

sa1100usb_core.o: $(sa1100usb_core-objs)
	$(LD) -r -o $@ $(sa1100usb_core-objs)

