# File: drivers/net/Makefile
#
# Makefile for the Linux network (ethercard) device drivers.
#

obj-y           :=
obj-m           :=
obj-n           :=
obj-            :=

mod-subdirs     := appletalk arcnet fc irda tokenring pcmcia wireless wan

O_TARGET := net.o

# All of the (potential) objects that export symbols.
# This list comes from 'grep -l EXPORT_SYMBOL *.[hc]'.

export-objs     :=	8390.o arlan.o aironet4500_core.o aironet4500_card.o \
			ppp_async.o ppp_generic.o slhc.o pppox.o auto_irq.o \
			net_init.o mii.o
list-multi	:=	rcpci.o
rcpci-objs	:=	rcpci45.o rclanmtl.o

ifeq ($(CONFIG_TULIP),y)
  obj-y += tulip/tulip.o
endif

obj-$(CONFIG_OCP_NET) += ibm_emac/ocp.o

ifeq ($(CONFIG_E1000),y)
  obj-y += e1000/e1000.o
endif

ifeq ($(CONFIG_BONDING),y)
  obj-y += bonding/bonding.o
endif

ifeq ($(CONFIG_ISDN_PPP),y)
  obj-$(CONFIG_ISDN) += slhc.o
endif

subdir-$(CONFIG_NET_PCMCIA) += pcmcia
subdir-$(CONFIG_NET_WIRELESS) += wireless
subdir-$(CONFIG_TULIP) += tulip
subdir-$(CONFIG_OCP_NET) += ibm_emac
subdir-$(CONFIG_IRDA) += irda
subdir-$(CONFIG_TR) += tokenring
subdir-$(CONFIG_WAN) += wan
subdir-$(CONFIG_NET_FC) += fc
subdir-$(CONFIG_ARCNET) += arcnet
subdir-$(CONFIG_DEV_APPLETALK) += appletalk
subdir-$(CONFIG_SK98LIN) += sk98lin
subdir-$(CONFIG_SKFP) += skfp
subdir-$(CONFIG_E100) += e100
subdir-$(CONFIG_E1000) += e1000
subdir-$(CONFIG_BONDING) += bonding

#
# link order important here
#
obj-$(CONFIG_PLIP) += plip.o

obj-$(CONFIG_ROADRUNNER) += rrunner.o

obj-$(CONFIG_HAPPYMEAL) += sunhme.o
obj-$(CONFIG_SUNLANCE) += sunlance.o
obj-$(CONFIG_SUNQE) += sunqe.o
obj-$(CONFIG_SUNBMAC) += sunbmac.o
obj-$(CONFIG_MYRI_SBUS) += myri_sbus.o
obj-$(CONFIG_SUNGEM) += sungem.o

obj-$(CONFIG_MACE) += mace.o
obj-$(CONFIG_BMAC) += bmac.o
obj-$(CONFIG_GMAC) += gmac.o

obj-$(CONFIG_OAKNET) += oaknet.o 8390.o

obj-$(CONFIG_DGRS) += dgrs.o
obj-$(CONFIG_RCPCI) += rcpci.o
obj-$(CONFIG_VORTEX) += 3c59x.o mii.o
obj-$(CONFIG_TYPHOON) += typhoon.o
obj-$(CONFIG_NE2K_PCI) += ne2k-pci.o 8390.o
obj-$(CONFIG_PCNET32) += pcnet32.o mii.o
obj-$(CONFIG_EEPRO100) += eepro100.o mii.o
obj-$(CONFIG_TLAN) += tlan.o
obj-$(CONFIG_EPIC100) += epic100.o mii.o
obj-$(CONFIG_SIS900) += sis900.o mii.o
obj-$(CONFIG_DM9102) += dmfe.o
obj-$(CONFIG_YELLOWFIN) += yellowfin.o
obj-$(CONFIG_ACENIC) += acenic.o
obj-$(CONFIG_VETH) += veth.o
obj-$(CONFIG_NATSEMI) += natsemi.o
obj-$(CONFIG_NS83820) += ns83820.o
obj-$(CONFIG_STNIC) += stnic.o 8390.o
obj-$(CONFIG_FEALNX) += fealnx.o mii.o
obj-$(CONFIG_TC35815) += tc35815.o
obj-$(CONFIG_TIGON3) += tg3.o

ifeq ($(CONFIG_E100),y)
  obj-y += e100/e100.o
endif

ifeq ($(CONFIG_SK98LIN),y)
obj-y += sk98lin/sk98lin.o
endif

ifeq ($(CONFIG_SKFP),y)
  obj-y += skfp/skfp.o
endif

obj-$(CONFIG_VIA_RHINE) += via-rhine.o mii.o
obj-$(CONFIG_ADAPTEC_STARFIRE) += starfire.o mii.o

#
# end link order section
#

obj-$(CONFIG_GALILEO_64240_ETH) += gt64240eth.o
obj-$(CONFIG_MV64340_ETH)	+= mv64340_eth.o
obj-$(CONFIG_AIRONET4500) 	+= aironet4500_core.o
obj-$(CONFIG_AIRONET4500_CS) 	+= aironet4500_core.o
obj-$(CONFIG_AIRONET4500_NONCS)  += aironet4500_card.o
obj-$(CONFIG_AIRONET4500_PROC)	+= aironet4500_proc.o
obj-$(CONFIG_AIRONET4500_CS)	+= aironet4500_proc.o

obj-$(CONFIG_WINBOND_840) += winbond-840.o mii.o
obj-$(CONFIG_SUNDANCE) += sundance.o mii.o
obj-$(CONFIG_HAMACHI) += hamachi.o mii.o
obj-$(CONFIG_NET) += Space.o setup.o net_init.o loopback.o
obj-$(CONFIG_SEEQ8005) += seeq8005.o
obj-$(CONFIG_ETHERTAP) += ethertap.o
obj-$(CONFIG_NET_SB1000) += sb1000.o
obj-$(CONFIG_MAC8390) += mac8390.o 8390.o
obj-$(CONFIG_APNE) += apne.o 8390.o
obj-$(CONFIG_PCMCIA_PCNET) += 8390.o
obj-$(CONFIG_SHAPER) += shaper.o
obj-$(CONFIG_SK_G16) += sk_g16.o
obj-$(CONFIG_HP100) += hp100.o
obj-$(CONFIG_SMC9194) += smc9194.o
obj-$(CONFIG_ARM_AM79C961A) += am79c961a.o
obj-$(CONFIG_ARM_ETHERH) += 8390.o
obj-$(CONFIG_WD80x3) += wd.o 8390.o
obj-$(CONFIG_EL2) += 3c503.o 8390.o
obj-$(CONFIG_NE2000) += ne.o 8390.o
obj-$(CONFIG_NE2_MCA) += ne2.o 8390.o
obj-$(CONFIG_HPLAN) += hp.o 8390.o
obj-$(CONFIG_HPLAN_PLUS) += hp-plus.o 8390.o
obj-$(CONFIG_ULTRA) += smc-ultra.o 8390.o
obj-$(CONFIG_ULTRAMCA) += smc-mca.o 8390.o
obj-$(CONFIG_ULTRA32) += smc-ultra32.o 8390.o
obj-$(CONFIG_E2100) += e2100.o 8390.o
obj-$(CONFIG_ES3210) += es3210.o 8390.o
obj-$(CONFIG_LNE390) += lne390.o 8390.o
obj-$(CONFIG_NE3210) += ne3210.o 8390.o
obj-$(CONFIG_NET_SB1250_MAC) += sb1250-mac.o
obj-$(CONFIG_B44) += b44.o
obj-$(CONFIG_FORCEDETH) += forcedeth.o

obj-$(CONFIG_PPP) += ppp_generic.o slhc.o
obj-$(CONFIG_PPP_ASYNC) += ppp_async.o
obj-$(CONFIG_PPP_SYNC_TTY) += ppp_synctty.o
obj-$(CONFIG_PPP_DEFLATE) += ppp_deflate.o
obj-$(CONFIG_PPP_BSDCOMP) += bsd_comp.o
obj-$(CONFIG_PPPOE) += pppox.o pppoe.o

obj-$(CONFIG_SLIP) += slip.o
ifeq ($(CONFIG_SLIP_COMPRESSED),y)
  obj-$(CONFIG_SLIP) += slhc.o
endif

obj-$(CONFIG_STRIP) += strip.o
obj-$(CONFIG_DUMMY) += dummy.o
obj-$(CONFIG_DE600) += de600.o
obj-$(CONFIG_DE620) += de620.o
obj-$(CONFIG_AT1500) += lance.o
obj-$(CONFIG_LANCE) += lance.o
obj-$(CONFIG_SUN3_82586) += sun3_82586.o
obj-$(CONFIG_SUN3LANCE) += sun3lance.o
obj-$(CONFIG_DEFXX) += defxx.o
obj-$(CONFIG_SGISEEQ) += sgiseeq.o
obj-$(CONFIG_AT1700) += at1700.o
obj-$(CONFIG_FMV18X) += fmv18x.o
obj-$(CONFIG_EL1) += 3c501.o
obj-$(CONFIG_EL16) += 3c507.o
obj-$(CONFIG_ELMC) += 3c523.o
obj-$(CONFIG_SKMC) += sk_mca.o
obj-$(CONFIG_IBMLANA) += ibmlana.o
obj-$(CONFIG_ELMC_II) += 3c527.o
obj-$(CONFIG_EL3) += 3c509.o
obj-$(CONFIG_3C515) += 3c515.o
obj-$(CONFIG_EEXPRESS) += eexpress.o
obj-$(CONFIG_EEXPRESS_PRO) += eepro.o
obj-$(CONFIG_8139CP) += 8139cp.o mii.o
obj-$(CONFIG_8139TOO) += 8139too.o mii.o
obj-$(CONFIG_WAVELAN) += wavelan.o
obj-$(CONFIG_ARLAN) += arlan.o arlan-proc.o
obj-$(CONFIG_ZNET) += znet.o
obj-$(CONFIG_LAN_SAA9730) += saa9730.o
obj-$(CONFIG_DEPCA) += depca.o
obj-$(CONFIG_EWRK3) += ewrk3.o
obj-$(CONFIG_ATP) += atp.o
obj-$(CONFIG_DE4X5) += de4x5.o
obj-$(CONFIG_NI5010) += ni5010.o
obj-$(CONFIG_NI52) += ni52.o
obj-$(CONFIG_NI65) += ni65.o
obj-$(CONFIG_ELPLUS) += 3c505.o
obj-$(CONFIG_AC3200) += ac3200.o 8390.o
obj-$(CONFIG_APRICOT) += 82596.o
obj-$(CONFIG_LASI_82596) += lasi_82596.o
obj-$(CONFIG_MVME16x_NET) += 82596.o
obj-$(CONFIG_BVME6000_NET) += 82596.o

# This is also a 82596 and should probably be merged
obj-$(CONFIG_LP486E) += lp486e.o

obj-$(CONFIG_ETH16I) += eth16i.o
obj-$(CONFIG_ZORRO8390) += zorro8390.o 8390.o
obj-$(CONFIG_HPLANCE) += hplance.o 7990.o
obj-$(CONFIG_MVME147_NET) += mvme147.o 7990.o
obj-$(CONFIG_EQUALIZER) += eql.o
obj-$(CONFIG_MIPS_JAZZ_SONIC) += jazzsonic.o
obj-$(CONFIG_MIPS_GT96100ETH) += gt96100eth.o
obj-$(CONFIG_MIPS_AU1X00_ENET) += au1000_eth.o
obj-$(CONFIG_SGI_IOC3_ETH) += ioc3-eth.o
obj-$(CONFIG_BAGETLANCE) += bagetlance.o
obj-$(CONFIG_DECLANCE) += declance.o
obj-$(CONFIG_ATARILANCE) += atarilance.o
obj-$(CONFIG_ATARI_BIONET) += atari_bionet.o
obj-$(CONFIG_ATARI_PAMSNET) += atari_pamsnet.o
obj-$(CONFIG_A2065) += a2065.o
obj-$(CONFIG_HYDRA) += hydra.o 8390.o
obj-$(CONFIG_ARIADNE) += ariadne.o
obj-$(CONFIG_CS89x0) += cs89x0.o
obj-$(CONFIG_ARM_CIRRUS) += cirrus.o
obj-$(CONFIG_MACSONIC) += macsonic.o
obj-$(CONFIG_MACMACE) += macmace.o
obj-$(CONFIG_MAC89x0) += mac89x0.o
obj-$(CONFIG_TUN) += tun.o
obj-$(CONFIG_ETHER00) +=ether00.o
obj-$(CONFIG_DL2K) += dl2k.o
obj-$(CONFIG_R8169) += r8169.o
obj-$(CONFIG_AMD8111_ETH) += amd8111e.o mii.o

# non-drivers/net drivers who want mii lib
obj-$(CONFIG_PCMCIA_SMC91C92) += mii.o
obj-$(CONFIG_USB_USBNET) += mii.o

ifeq ($(CONFIG_ARCH_ACORN),y)
mod-subdirs	+= ../acorn/net
subdir-y	+= ../acorn/net
obj-y		+= ../acorn/net/acorn-net.o
endif

#
# HIPPI adapters
#

ifneq ($(ARCH),s390)
ifneq ($(ARCH),s390x)
obj-y += auto_irq.o
endif
endif

include $(TOPDIR)/Rules.make

clean:
	rm -f core *.o *.a *.s

rcpci.o: $(rcpci-objs)
	$(LD) -r -o $@ $(rcpci-objs)

