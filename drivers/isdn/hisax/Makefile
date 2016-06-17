# Makefile for the hisax ISDN device driver

# The target object and module list name.

O_TARGET	  := vmlinux-obj.o

# Define maximum number of cards

EXTRA_CFLAGS      += -DHISAX_MAX_CARDS=$(CONFIG_HISAX_MAX_CARDS)

# Objects that export symbols.

export-objs	  := config.o hisax_isac.o isdnhdlc.o

# Multipart objects.

list-multi	  := hisax.o hisax_st5481.o
hisax-objs	  := config.o isdnl1.o tei.o isdnl2.o isdnl3.o \
		     lmgr.o q931.o callc.o fsm.o cert.o
hisax_st5481-objs := st5481_init.o st5481_usb.o st5481_d.o st5481_b.o

# Optional parts of multipart objects.
hisax-objs-$(CONFIG_HISAX_EURO) += l3dss1.o
hisax-objs-$(CONFIG_HISAX_NI1) += l3ni1.o
hisax-objs-$(CONFIG_HISAX_1TR6) += l3_1tr6.o

hisax-objs-$(CONFIG_HISAX_16_0) += teles0.o isac.o arcofi.o hscx.o
hisax-objs-$(CONFIG_HISAX_16_3) += teles3.o isac.o arcofi.o hscx.o
hisax-objs-$(CONFIG_HISAX_TELESPCI) += telespci.o isac.o arcofi.o hscx.o
hisax-objs-$(CONFIG_HISAX_S0BOX) += s0box.o isac.o arcofi.o hscx.o
hisax-objs-$(CONFIG_HISAX_AVM_A1) += avm_a1.o isac.o arcofi.o hscx.o
hisax-objs-$(CONFIG_HISAX_AVM_A1_PCMCIA) += avm_a1p.o isac.o arcofi.o hscx.o
hisax-objs-$(CONFIG_HISAX_FRITZPCI) += avm_pci.o isac.o arcofi.o
hisax-objs-$(CONFIG_HISAX_ELSA) += elsa.o isac.o arcofi.o hscx.o
hisax-objs-$(CONFIG_HISAX_IX1MICROR2) += ix1_micro.o isac.o arcofi.o hscx.o
hisax-objs-$(CONFIG_HISAX_DIEHLDIVA) += diva.o isac.o arcofi.o hscx.o ipacx.o
hisax-objs-$(CONFIG_HISAX_ASUSCOM) += asuscom.o isac.o arcofi.o hscx.o
hisax-objs-$(CONFIG_HISAX_TELEINT) += teleint.o isac.o arcofi.o hfc_2bs0.o
hisax-objs-$(CONFIG_HISAX_SEDLBAUER) += sedlbauer.o isac.o arcofi.o hscx.o isar.o
hisax-objs-$(CONFIG_HISAX_SPORTSTER) += sportster.o isac.o arcofi.o hscx.o
hisax-objs-$(CONFIG_HISAX_MIC) += mic.o isac.o arcofi.o hscx.o
hisax-objs-$(CONFIG_HISAX_NETJET) += nj_s.o netjet.o isac.o arcofi.o
hisax-objs-$(CONFIG_HISAX_NETJET_U) += nj_u.o netjet.o icc.o
hisax-objs-$(CONFIG_HISAX_HFCS) += hfcscard.o hfc_2bds0.o
hisax-objs-$(CONFIG_HISAX_HFC_PCI) += hfc_pci.o
hisax-objs-$(CONFIG_HISAX_HFC_SX) += hfc_sx.o
hisax-objs-$(CONFIG_HISAX_NICCY) += niccy.o isac.o arcofi.o hscx.o
hisax-objs-$(CONFIG_HISAX_ISURF) += isurf.o isac.o arcofi.o isar.o
hisax-objs-$(CONFIG_HISAX_HSTSAPHIR) += saphir.o isac.o arcofi.o hscx.o
hisax-objs-$(CONFIG_HISAX_BKM_A4T) += bkm_a4t.o isac.o arcofi.o jade.o
hisax-objs-$(CONFIG_HISAX_SCT_QUADRO) += bkm_a8.o isac.o arcofi.o hscx.o
hisax-objs-$(CONFIG_HISAX_GAZEL) += gazel.o isac.o arcofi.o hscx.o
hisax-objs-$(CONFIG_HISAX_W6692) += w6692.o
hisax-objs-$(CONFIG_HISAX_ENTERNOW_PCI) += enternow_pci.o amd7930_fn.o
#hisax-objs-$(CONFIG_HISAX_TESTEMU) += testemu.o

hisax-objs += $(sort $(hisax-objs-y))

# Each configuration option enables a list of files.

obj-$(CONFIG_ISDN_DRV_HISAX)		+= hisax.o
obj-$(CONFIG_HISAX_SEDLBAUER_CS)	+= sedlbauer_cs.o
obj-$(CONFIG_HISAX_ELSA_CS)		+= elsa_cs.o
obj-$(CONFIG_HISAX_AVM_A1_CS)		+= avma1_cs.o
obj-$(CONFIG_HISAX_ST5481)		+= hisax_st5481.o isdnhdlc.o
obj-$(CONFIG_HISAX_FRITZ_PCIPNP)        += hisax_isac.o hisax_fcpcipnp.o
obj-$(CONFIG_USB_AUERISDN)	        += isdnhdlc.o

CERT := $(shell md5sum -c md5sums.asc >> /dev/null;echo $$?)
CFLAGS_cert.o := -DCERTIFICATION=$(CERT)

include $(TOPDIR)/Rules.make

# Link rules for multi-part drivers.

hisax.o: $(hisax-objs)
	$(LD) -r -o $@ $(hisax-objs)

hisax_st5481.o: $(hisax_st5481-objs)
	$(LD) -r -o $@ $(hisax_st5481-objs)
