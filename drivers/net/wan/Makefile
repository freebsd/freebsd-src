#
# Makefile for the Linux network (wan) device drivers.
#
# 3 Aug 2000, Christoph Hellwig <hch@infradead.org>
# Rewritten to use lists instead of if-statements.
#

SUB_DIRS     := 

O_TARGET := wan.o

export-objs =	z85230.o syncppp.o comx.o sdladrv.o cycx_drv.o hdlc_generic.o pc300_drv.o
list-multi =	wanpipe.o cyclomx.o

wanpipe-objs = sdlamain.o sdla_ft1.o $(wanpipe-y)
wanpipe-$(CONFIG_WANPIPE_X25)	+= sdla_x25.o
wanpipe-$(CONFIG_WANPIPE_FR)	+= sdla_fr.o
wanpipe-$(CONFIG_WANPIPE_CHDLC)	+= sdla_chdlc.o
wanpipe-$(CONFIG_WANPIPE_PPP)	+= sdla_ppp.o
wanpipe-$(CONFIG_WANPIPE_MULTPPP) += wanpipe_multppp.o

cyclomx-objs = cycx_main.o $(cyclomx-y)  
cyclomx-$(CONFIG_CYCLOMX_X25)	+= cycx_x25.o

hdlc-y				:= hdlc_generic.o
hdlc-$(CONFIG_HDLC_RAW)		+= hdlc_raw.o
hdlc-$(CONFIG_HDLC_RAW_ETH)	+= hdlc_raw_eth.o
hdlc-$(CONFIG_HDLC_CISCO)	+= hdlc_cisco.o
hdlc-$(CONFIG_HDLC_FR)		+= hdlc_fr.o
hdlc-$(CONFIG_HDLC_PPP)		+= hdlc_ppp.o
hdlc-$(CONFIG_HDLC_X25)		+= hdlc_x25.o
hdlc-objs			:= $(hdlc-y)

pc300-$(CONFIG_PC300_MLPPP)	+= pc300_tty.o

obj-$(CONFIG_HOSTESS_SV11)	+= z85230.o	syncppp.o	hostess_sv11.o
obj-$(CONFIG_SEALEVEL_4021)	+= z85230.o	syncppp.o	sealevel.o
obj-$(CONFIG_COMX)		+= 				comx.o
obj-$(CONFIG_COMX_HW_COMX)	+= 				comx-hw-comx.o
obj-$(CONFIG_COMX_HW_LOCOMX)	+= z85230.o	syncppp.o	comx-hw-locomx.o
obj-$(CONFIG_COMX_HW_MIXCOM)	+=				comx-hw-mixcom.o
obj-$(CONFIG_COMX_HW_MUNICH)	+=				comx-hw-munich.o
obj-$(CONFIG_COMX_PROTO_PPP)	+=		syncppp.o	comx-proto-ppp.o
obj-$(CONFIG_COMX_PROTO_LAPB)	+=				comx-proto-lapb.o
obj-$(CONFIG_COMX_PROTO_FR)	+=				comx-proto-fr.o
obj-$(CONFIG_COSA)		+=		syncppp.o	cosa.o
obj-$(CONFIG_FARSYNC)		+=		syncppp.o	farsync.o
obj-$(CONFIG_DSCC4)             +=				dscc4.o
obj-$(CONFIG_LANMEDIA)		+=		syncppp.o
obj-$(CONFIG_SYNCLINK_SYNCPPP)	+=		syncppp.o
obj-$(CONFIG_X25_ASY)		+= x25_asy.o

subdir-$(CONFIG_LANMEDIA) += lmc

ifeq ($(CONFIG_LANMEDIA),y)
  obj-y += lmc/lmc.o
endif

subdir-$(CONFIG_ATI_XX20) += 8253x

ifeq ($(CONFIG_ATI_XX20),y)
  obj-y += 8253x/ASLX.o
endif

 
obj-$(CONFIG_DLCI)		+= dlci.o 
obj-$(CONFIG_SDLA)		+= sdla.o
ifeq ($(CONFIG_WANPIPE_MULTPPP),y)
  obj-$(CONFIG_VENDOR_SANGOMA) += sdladrv.o wanpipe.o syncppp.o	
else
  obj-$(CONFIG_VENDOR_SANGOMA) += sdladrv.o wanpipe.o
endif
obj-$(CONFIG_CYCLADES_SYNC)	+= cycx_drv.o cyclomx.o
obj-$(CONFIG_LAPBETHER)		+= lapbether.o
obj-$(CONFIG_SBNI)		+= sbni.o
obj-$(CONFIG_PC300)		+= pc300.o
obj-$(CONFIG_HDLC)		+= hdlc.o
ifeq ($(CONFIG_HDLC_PPP),y)
  obj-$(CONFIG_HDLC)		+= syncppp.o
endif
obj-$(CONFIG_N2)		+= n2.o
obj-$(CONFIG_C101)		+= c101.o
obj-$(CONFIG_PCI200SYN)		+= pci200syn.o

include $(TOPDIR)/Rules.make

pc300.o: pc300_drv.o $(pc300-y)
	$(LD) -r -o $@ pc300_drv.o $(pc300-y)

hdlc.o: $(hdlc-objs)
	$(LD) -r -o $@ $(hdlc-objs)

wanpipe.o: $(wanpipe-objs)
	$(LD) -r -o $@ $(wanpipe-objs)

cyclomx.o: $(cyclomx-objs)
	$(LD) -r -o $@ $(cyclomx-objs)

