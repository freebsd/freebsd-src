#
# Makefile for the kernel character device drivers.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now inherited from the
# parent makes..
#

#
# This file contains the font map for the default (hardware) font
#
FONTMAPFILE = cp437.uni

O_TARGET := char.o

obj-y	 += mem.o tty_io.o n_tty.o tty_ioctl.o raw.o pty.o misc.o random.o

# All of the (potential) objects that export symbols.
# This list comes from 'grep -l EXPORT_SYMBOL *.[hc]'.

export-objs     :=	busmouse.o console.o keyboard.o sysrq.o \
			misc.o pty.o random.o selection.o serial.o \
			sonypi.o tty_io.o tty_ioctl.o generic_serial.o \
			au1000_gpio.o vac-serial.o hp_psaux.o nvram.o \
			scx200.o fetchop.o

mod-subdirs	:=	joystick ftape drm drm-4.0 pcmcia

list-multi	:=	

KEYMAP   =defkeymap.o
KEYBD    =pc_keyb.o
CONSOLE  =console.o
SERIAL   =serial.o

ifeq ($(ARCH),s390)
  KEYMAP   =
  KEYBD    =
  CONSOLE  =
  SERIAL   =
endif

ifeq ($(ARCH),mips)
  ifneq ($(CONFIG_PC_KEYB),y)
    KEYBD    =
  endif
  ifeq ($(CONFIG_VR41XX_KIU),y)
    KEYMAP   =
    KEYBD    = vr41xx_keyb.o
  endif
endif

ifeq ($(ARCH),s390x)
  KEYMAP   =
  KEYBD    =
  CONSOLE  =
  SERIAL   =
endif

ifeq ($(ARCH),m68k)
   ifdef CONFIG_AMIGA
      KEYBD = amikeyb.o
   else
      ifndef CONFIG_MAC
	 KEYBD =
      endif
   endif
   SERIAL   =
endif

ifeq ($(ARCH),parisc)
   ifdef CONFIG_GSC_PS2
      KEYBD   = hp_psaux.o hp_keyb.o
   else
      KEYBD   =
   endif
   ifdef CONFIG_SERIAL_MUX
      CONSOLE += mux.o
   endif
   ifdef CONFIG_PDC_CONSOLE
      CONSOLE += pdc_console.o
   endif
endif

ifdef CONFIG_Q40
  KEYBD += q40_keyb.o
  SERIAL = serial.o
endif

ifdef CONFIG_APOLLO
  KEYBD += dn_keyb.o
endif

ifeq ($(ARCH),parisc)
   ifdef CONFIG_GSC_PS2
      KEYBD   = hp_psaux.o hp_keyb.o
   else
      KEYBD   =
   endif
   ifdef CONFIG_PDC_CONSOLE
      CONSOLE += pdc_console.o
   endif
endif

ifeq ($(ARCH),arm)
  ifneq ($(CONFIG_PC_KEYMAP),y)
    KEYMAP   =
  endif
  ifneq ($(CONFIG_PC_KEYB),y)
    KEYBD    =
  endif
endif

ifeq ($(ARCH),sh)
  KEYMAP   =
  KEYBD    =
  CONSOLE  =
  ifeq ($(CONFIG_SH_HP600),y)
  KEYMAP   = defkeymap.o
  KEYBD    = scan_keyb.o hp600_keyb.o
  CONSOLE  = console.o
  endif
  ifeq ($(CONFIG_SH_DMIDA),y)
  # DMIDA does not connect the HD64465 PS/2 keyboard port
  # but we allow for USB keyboards to be plugged in.
  KEYMAP   = defkeymap.o
  KEYBD    = # hd64465_keyb.o pc_keyb.o
  CONSOLE  = console.o
  endif
  ifeq ($(CONFIG_SH_EC3104),y)
  KEYMAP   = defkeymap.o
  KEYBD    = ec3104_keyb.o
  CONSOLE  = console.o
  endif
  ifeq ($(CONFIG_SH_DREAMCAST),y)
  KEYMAP   = defkeymap.o
  KEYBD    =
  CONSOLE  = console.o
  endif
endif

ifeq ($(CONFIG_DECSTATION),y)
  KEYMAP   =
  KEYBD    =
endif

ifeq ($(CONFIG_BAGET_MIPS),y)
  KEYBD    =
  SERIAL   = vac-serial.o
endif

ifeq ($(CONFIG_NINO),y)
  SERIAL   =
endif

ifneq ($(CONFIG_SUN_SERIAL),)
  SERIAL   =
endif

ifeq ($(CONFIG_QTRONIX_KEYBOARD),y)
  KEYBD    = qtronix.o
  KEYMAP   = qtronixmap.o
endif

ifeq ($(CONFIG_DUMMY_KEYB),y)
  KEYBD = dummy_keyb.o
endif

obj-$(CONFIG_VT) += vt.o vc_screen.o consolemap.o consolemap_deftbl.o $(CONSOLE) selection.o
obj-$(CONFIG_SERIAL) += $(SERIAL)
obj-$(CONFIG_SERIAL_HCDP) += hcdp_serial.o
obj-$(CONFIG_SERIAL_21285) += serial_21285.o
obj-$(CONFIG_SERIAL_SA1100) += serial_sa1100.o
obj-$(CONFIG_SERIAL_AMBA) += serial_amba.o
obj-$(CONFIG_TS_AU1X00_ADS7846) += au1000_ts.o
obj-$(CONFIG_SERIAL_DEC) += decserial.o

ifndef CONFIG_SUN_KEYBOARD
  obj-$(CONFIG_VT) += keyboard.o $(KEYMAP) $(KEYBD)
else
  obj-$(CONFIG_PCI) += keyboard.o $(KEYMAP)
endif

obj-$(CONFIG_HIL) += hp_keyb.o
obj-$(CONFIG_MAGIC_SYSRQ) += sysrq.o
obj-$(CONFIG_ATARI_DSP56K) += dsp56k.o
obj-$(CONFIG_ROCKETPORT) += rocket.o
obj-$(CONFIG_MOXA_SMARTIO) += mxser.o
obj-$(CONFIG_MOXA_INTELLIO) += moxa.o
obj-$(CONFIG_DIGI) += pcxx.o
obj-$(CONFIG_DIGIEPCA) += epca.o
obj-$(CONFIG_CYCLADES) += cyclades.o
obj-$(CONFIG_STALLION) += stallion.o
obj-$(CONFIG_ISTALLION) += istallion.o
obj-$(CONFIG_SIBYTE_SB1250_DUART) += sb1250_duart.o
obj-$(CONFIG_COMPUTONE) += ip2.o ip2main.o
obj-$(CONFIG_RISCOM8) += riscom8.o
obj-$(CONFIG_ISI) += isicom.o
obj-$(CONFIG_ESPSERIAL) += esp.o
obj-$(CONFIG_SYNCLINK) += synclink.o
obj-$(CONFIG_SYNCLINKMP) += synclinkmp.o
obj-$(CONFIG_N_HDLC) += n_hdlc.o
obj-$(CONFIG_SPECIALIX) += specialix.o
obj-$(CONFIG_AMIGA_BUILTIN_SERIAL) += amiserial.o
obj-$(CONFIG_A2232) += ser_a2232.o generic_serial.o
obj-$(CONFIG_SX) += sx.o generic_serial.o
obj-$(CONFIG_RIO) += rio/rio.o generic_serial.o
obj-$(CONFIG_SH_SCI) += sh-sci.o generic_serial.o
obj-$(CONFIG_SERIAL167) += serial167.o
obj-$(CONFIG_MVME147_SCC) += generic_serial.o vme_scc.o
obj-$(CONFIG_MVME162_SCC) += generic_serial.o vme_scc.o
obj-$(CONFIG_BVME6000_SCC) += generic_serial.o vme_scc.o
obj-$(CONFIG_HVC_CONSOLE) += hvc_console.o
obj-$(CONFIG_SERIAL_TX3912) += generic_serial.o serial_tx3912.o
obj-$(CONFIG_TXX927_SERIAL) += serial_txx927.o
obj-$(CONFIG_SERIAL_TXX9) += generic_serial.o serial_txx9.o
obj-$(CONFIG_IP22_SERIAL) += sgiserial.o
obj-$(CONFIG_AU1X00_UART) += au1x00-serial.o
obj-$(CONFIG_SGI_L1_SERIAL) += sn_serial.o

subdir-$(CONFIG_RIO) += rio
subdir-$(CONFIG_INPUT) += joystick

obj-$(CONFIG_ATIXL_BUSMOUSE) += atixlmouse.o
obj-$(CONFIG_LOGIBUSMOUSE) += logibusmouse.o
obj-$(CONFIG_PRINTER) += lp.o
obj-$(CONFIG_TIPAR) += tipar.o
obj-$(CONFIG_OBMOUSE) += obmouse.o

ifeq ($(CONFIG_INPUT),y)
obj-y += joystick/js.o
endif

obj-$(CONFIG_FETCHOP) += fetchop.o
obj-$(CONFIG_BUSMOUSE) += busmouse.o
obj-$(CONFIG_DTLK) += dtlk.o
obj-$(CONFIG_R3964) += n_r3964.o
obj-$(CONFIG_APPLICOM) += applicom.o
obj-$(CONFIG_SONYPI) += sonypi.o
obj-$(CONFIG_MS_BUSMOUSE) += msbusmouse.o
obj-$(CONFIG_82C710_MOUSE) += qpmouse.o
obj-$(CONFIG_AMIGAMOUSE) += amigamouse.o
obj-$(CONFIG_ATARIMOUSE) += atarimouse.o
obj-$(CONFIG_ADBMOUSE) += adbmouse.o
obj-$(CONFIG_PC110_PAD) += pc110pad.o
obj-$(CONFIG_MK712_MOUSE) += mk712.o
obj-$(CONFIG_RTC) += rtc.o
obj-$(CONFIG_GEN_RTC) += genrtc.o
obj-$(CONFIG_EFI_RTC) += efirtc.o
obj-$(CONFIG_SGI_DS1286) += ds1286.o
obj-$(CONFIG_MIPS_RTC) += mips_rtc.o
obj-$(CONFIG_SGI_IP27_RTC) += ip27-rtc.o
ifeq ($(CONFIG_PPC),)
  obj-$(CONFIG_NVRAM) += nvram.o
endif
obj-$(CONFIG_TOSHIBA) += toshiba.o
obj-$(CONFIG_I8K) += i8k.o
obj-$(CONFIG_DS1620) += ds1620.o
obj-$(CONFIG_DS1742) += ds1742.o
obj-$(CONFIG_INTEL_RNG) += i810_rng.o
obj-$(CONFIG_AMD_RNG) += amd768_rng.o
obj-$(CONFIG_HW_RANDOM) += hw_random.o
obj-$(CONFIG_AMD_PM768) += amd76x_pm.o
obj-$(CONFIG_BRIQ_PANEL) += briq_panel.o

obj-$(CONFIG_ITE_GPIO) += ite_gpio.o
obj-$(CONFIG_AU1X00_GPIO) += au1000_gpio.o
obj-$(CONFIG_AU1X00_USB_TTY) += au1000_usbtty.o
obj-$(CONFIG_AU1X00_USB_RAW) += au1000_usbraw.o
obj-$(CONFIG_COBALT_LCD) += lcd.o

obj-$(CONFIG_QIC02_TAPE) += tpqic02.o

subdir-$(CONFIG_FTAPE) += ftape
subdir-$(CONFIG_DRM_OLD) += drm-4.0
subdir-$(CONFIG_DRM_NEW) += drm
subdir-$(CONFIG_PCMCIA) += pcmcia
subdir-$(CONFIG_AGP) += agp

ifeq ($(CONFIG_FTAPE),y)
obj-y       += ftape/ftape.o
endif

obj-$(CONFIG_H8) += h8.o
obj-$(CONFIG_PPDEV) += ppdev.o
obj-$(CONFIG_DZ) += dz.o
obj-$(CONFIG_NWBUTTON) += nwbutton.o
obj-$(CONFIG_NWFLASH) += nwflash.o
obj-$(CONFIG_SCx200) += scx200.o
obj-$(CONFIG_SCx200_GPIO) += scx200_gpio.o

# Only one watchdog can succeed. We probe the hardware watchdog
# drivers first, then the softdog driver.  This means if your hardware
# watchdog dies or is 'borrowed' for some reason the software watchdog
# still gives you some cover.

obj-$(CONFIG_PCWATCHDOG) += pcwd.o
obj-$(CONFIG_ACQUIRE_WDT) += acquirewdt.o
obj-$(CONFIG_ADVANTECH_WDT) += advantechwdt.o
obj-$(CONFIG_IB700_WDT) += ib700wdt.o
obj-$(CONFIG_MIXCOMWD) += mixcomwd.o
obj-$(CONFIG_60XX_WDT) += sbc60xxwdt.o
obj-$(CONFIG_W83877F_WDT) += w83877f_wdt.o
obj-$(CONFIG_SC520_WDT) += sc520_wdt.o
obj-$(CONFIG_WDT) += wdt.o
obj-$(CONFIG_WDTPCI) += wdt_pci.o
obj-$(CONFIG_21285_WATCHDOG) += wdt285.o
obj-$(CONFIG_977_WATCHDOG) += wdt977.o
obj-$(CONFIG_I810_TCO) += i810-tco.o
obj-$(CONFIG_MACHZ_WDT) += machzwd.o
obj-$(CONFIG_SH_WDT) += shwdt.o
obj-$(CONFIG_EUROTECH_WDT) += eurotechwdt.o
obj-$(CONFIG_ALIM7101_WDT) += alim7101_wdt.o
obj-$(CONFIG_ALIM1535_WDT) += alim1535d_wdt.o
obj-$(CONFIG_INDYDOG) += indydog.o
obj-$(CONFIG_SC1200_WDT) += sc1200wdt.o
obj-$(CONFIG_SCx200_WDT) += scx200_wdt.o
obj-$(CONFIG_WAFER_WDT) += wafer5823wdt.o
obj-$(CONFIG_SOFT_WATCHDOG) += softdog.o
obj-$(CONFIG_AMD7XX_TCO) += amd7xx_tco.o
obj-$(CONFIG_INDYDOG) += indydog.o
obj-$(CONFIG_8xx_WDT) += mpc8xx_wdt.o

subdir-$(CONFIG_MWAVE) += mwave
ifeq ($(CONFIG_MWAVE),y)
  obj-y += mwave/mwave.o
endif

subdir-$(CONFIG_IPMI_HANDLER) += ipmi
ifeq ($(CONFIG_IPMI_HANDLER),y)
  obj-y += ipmi/ipmi.o
endif

include $(TOPDIR)/Rules.make

fastdep:

conmakehash: conmakehash.c
	$(HOSTCC) $(HOSTCFLAGS) -o conmakehash conmakehash.c

consolemap_deftbl.c: $(FONTMAPFILE) conmakehash
	./conmakehash $(FONTMAPFILE) > consolemap_deftbl.c

consolemap_deftbl.o: consolemap_deftbl.c $(TOPDIR)/include/linux/types.h

.DELETE_ON_ERROR:

defkeymap.c: defkeymap.map
	set -e ; loadkeys --mktable $< | sed -e 's/^static *//' > $@

qtronixmap.c: qtronixmap.map
	set -e ; loadkeys --mktable $< | sed -e 's/^static *//' > $@
