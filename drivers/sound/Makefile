# Makefile for the Linux sound card driver
#
# 18 Apr 1998, Michael Elizabeth Chastain, <mailto:mec@shout.net>
# Rewritten to use lists instead of if-statements.


# All of the (potential) objects that export symbols.
# This list comes from 'grep -l EXPORT_SYMBOL *.[hc]'.

export-objs	:=  ad1848.o audio_syms.o midi_syms.o mpu401.o \
		    msnd.o opl3.o sb_common.o sequencer_syms.o \
		    sound_core.o sound_syms.o uart401.o	\
		    nm256_audio.o ac97.o ac97_codec.o aci.o

# Each configuration option enables a list of files.

obj-$(CONFIG_SOUND)		+= soundcore.o
obj-$(CONFIG_SOUND_OSS)		+= sound.o
obj-$(CONFIG_SOUND_CS4232)	+= cs4232.o ad1848.o 

# Please leave it as is, cause the link order is significant !

obj-$(CONFIG_SOUND_HAL2)	+= hal2.o
obj-$(CONFIG_SOUND_AEDSP16)	+= aedsp16.o
obj-$(CONFIG_SOUND_ALI5455)	+= ali5455.o ac97_codec.o
obj-$(CONFIG_SOUND_PSS)		+= pss.o ad1848.o mpu401.o
obj-$(CONFIG_SOUND_TRIX)	+= trix.o ad1848.o sb_lib.o uart401.o
obj-$(CONFIG_SOUND_OPL3SA1)	+= opl3sa.o ad1848.o uart401.o
obj-$(CONFIG_SOUND_SSCAPE)	+= sscape.o ad1848.o mpu401.o
obj-$(CONFIG_SOUND_MAD16)	+= mad16.o ad1848.o sb_lib.o uart401.o
obj-$(CONFIG_SOUND_CS4232)	+= cs4232.o uart401.o
obj-$(CONFIG_SOUND_OPL3SA2)	+= opl3sa2.o ad1848.o mpu401.o
obj-$(CONFIG_SOUND_MSS)		+= ad1848.o
obj-$(CONFIG_SOUND_PAS)		+= pas2.o sb.o sb_lib.o uart401.o
obj-$(CONFIG_SOUND_SB)		+= sb.o sb_lib.o uart401.o
obj-$(CONFIG_SOUND_KAHLUA)	+= kahlua.o
obj-$(CONFIG_SOUND_WAVEFRONT)	+= wavefront.o
obj-$(CONFIG_SOUND_MAUI)	+= maui.o mpu401.o
obj-$(CONFIG_SOUND_MPU401)	+= mpu401.o
obj-$(CONFIG_SOUND_UART6850)	+= uart6850.o
obj-$(CONFIG_SOUND_GUS)		+= gus.o ad1848.o
obj-$(CONFIG_SOUND_ADLIB)	+= adlib_card.o opl3.o
obj-$(CONFIG_SOUND_YM3812)	+= opl3.o
obj-$(CONFIG_SOUND_VMIDI)	+= v_midi.o
obj-$(CONFIG_SOUND_VIDC)	+= vidc_mod.o
obj-$(CONFIG_SOUND_WAVEARTIST)	+= waveartist.o
obj-$(CONFIG_SOUND_SGALAXY)	+= sgalaxy.o ad1848.o
obj-$(CONFIG_SOUND_AD1816)	+= ad1816.o
obj-$(CONFIG_SOUND_AD1889)      += ad1889.o ac97_codec.o
obj-$(CONFIG_SOUND_ACI_MIXER)	+= aci.o
obj-$(CONFIG_SOUND_AWE32_SYNTH)	+= awe_wave.o

obj-$(CONFIG_SOUND_VIA82CXXX)	+= via82cxxx_audio.o ac97_codec.o
ifeq ($(CONFIG_MIDI_VIA82CXXX),y)
  obj-$(CONFIG_SOUND_VIA82CXXX) += sound.o uart401.o
endif
obj-$(CONFIG_SOUND_YMFPCI)	+= ymfpci.o ac97_codec.o
ifeq ($(CONFIG_SOUND_YMFPCI_LEGACY),y)
  obj-$(CONFIG_SOUND_YMFPCI)    += opl3.o uart401.o
endif
obj-$(CONFIG_SOUND_MSNDCLAS)	+= msnd.o msnd_classic.o
obj-$(CONFIG_SOUND_MSNDPIN)	+= msnd.o msnd_pinnacle.o
obj-$(CONFIG_SOUND_VWSND)	+= vwsnd.o
obj-$(CONFIG_SOUND_NM256)	+= nm256_audio.o ac97.o
obj-$(CONFIG_SOUND_ICH)		+= i810_audio.o ac97_codec.o
obj-$(CONFIG_SOUND_SONICVIBES)	+= sonicvibes.o
obj-$(CONFIG_SOUND_CMPCI)	+= cmpci.o
obj-$(CONFIG_SOUND_ES1370)	+= es1370.o
obj-$(CONFIG_SOUND_ES1371)	+= es1371.o ac97_codec.o
obj-$(CONFIG_SOUND_VRC5477)	+= nec_vrc5477.o ac97_codec.o
obj-$(CONFIG_SOUND_AU1X00)      += au1000.o ac97_codec.o  
obj-$(CONFIG_SOUND_ESSSOLO1)	+= esssolo1.o
obj-$(CONFIG_SOUND_FUSION)	+= cs46xx.o ac97_codec.o
obj-$(CONFIG_SOUND_MAESTRO)	+= maestro.o
obj-$(CONFIG_SOUND_MAESTRO3)	+= maestro3.o ac97_codec.o
obj-$(CONFIG_SOUND_FORTE)	+= forte.o ac97_codec.o
obj-$(CONFIG_SOUND_TRIDENT)	+= trident.o ac97_codec.o
obj-$(CONFIG_SOUND_HARMONY)	+= harmony.o
obj-$(CONFIG_SOUND_EMU10K1)	+= ac97_codec.o
obj-$(CONFIG_SOUND_BCM_CS4297A)	+= swarm_cs4297a.o
obj-$(CONFIG_SOUND_RME96XX)     += rme96xx.o
obj-$(CONFIG_SOUND_BT878)	+= btaudio.o
obj-$(CONFIG_SOUND_IT8172)	+= ite8172.o ac97_codec.o
obj-$(CONFIG_SOUND_AD1980)	+= ac97_plugin_ad1980.o
obj-$(CONFIG_SOUND_WM97XX)	+= ac97_plugin_wm97xx.o

ifeq ($(CONFIG_MIDI_EMU10K1),y)
  obj-$(CONFIG_SOUND_EMU10K1)	+= sound.o
endif

subdir-$(CONFIG_SOUND_EMU10K1) += emu10k1
subdir-$(CONFIG_SOUND_CS4281) += cs4281

ifeq ($(CONFIG_SOUND_EMU10K1),y)
  obj-y += emu10k1/emu10k1.o
endif

ifeq ($(CONFIG_SOUND_CS4281),y)
  obj-y += cs4281/cs4281.o
endif

subdir-$(CONFIG_DMASOUND) += dmasound

ifeq ($(CONFIG_DMASOUND),y)
  obj-y += dmasound/dmasound.o
endif


# Declare multi-part drivers.

list-multi	:= sound.o gus.o pas2.o sb.o sb_lib.o vidc_mod.o \
    soundcore.o wavefront.o

sound-objs	:= 							\
    dev_table.o soundcard.o sound_syms.o		\
    audio.o audio_syms.o dmabuf.o					\
    midi_syms.o midi_synth.o midibuf.o					\
    sequencer.o sequencer_syms.o sound_timer.o sys_timer.o

soundcore-objs	:= sound_core.o sound_firmware.o

gus-objs	:= gus_card.o gus_midi.o gus_vol.o gus_wave.o ics2101.o
pas2-objs	:= pas2_card.o pas2_midi.o pas2_mixer.o pas2_pcm.o
sb-objs		:= sb_card.o
sb_lib-objs	:= sb_common.o sb_audio.o sb_midi.o sb_mixer.o sb_ess.o
vidc_mod-objs	:= vidc.o vidc_fill.o
wavefront-objs  := wavfront.o wf_midi.o yss225.o


O_TARGET	:= sounddrivers.o

include $(TOPDIR)/Rules.make



# Link rules for multi-part drivers.

sound.o: $(sound-objs)
	$(LD) -r -o $@ $(sound-objs)

soundcore.o: $(soundcore-objs)
	$(LD) -r -o $@ $(soundcore-objs)

gus.o: $(gus-objs)
	$(LD) -r -o $@ $(gus-objs)

pas2.o: $(pas2-objs)
	$(LD) -r -o $@ $(pas2-objs)

sb.o: $(sb-objs)
	$(LD) -r -o $@ $(sb-objs)

sb_lib.o: $(sb_lib-objs)
	$(LD) -r -o $@ $(sb_lib-objs)

vidc_mod.o: $(vidc_mod-objs)
	$(LD) -r -o $@ $(vidc_mod-objs)

wavefront.o: $(wavefront-objs)
	$(LD) -r -o $@ $(wavefront-objs)

# Firmware files that need translation
#
# The translated files are protected by a file that keeps track
# of what name was used to build them.  If the name changes, they
# will be forced to be remade.
#
# First make the utilities.

bin2hex: bin2hex.c
	$(HOSTCC) $(HOSTCFLAGS) -o bin2hex bin2hex.c

hex2hex: hex2hex.c
	$(HOSTCC) $(HOSTCFLAGS) -o hex2hex hex2hex.c




# Turtle Beach Maui / Tropez

maui.o: maui_boot.h

ifeq ($(CONFIG_MAUI_HAVE_BOOT),y)
    maui_boot.h: $(patsubst "%", %, $(CONFIG_MAUI_BOOT_FILE)) bin2hex
	./bin2hex -i maui_os < $(CONFIG_MAUI_BOOT_FILE) > $@
else
    maui_boot.h:
	(							\
	    echo 'static unsigned char * maui_os = NULL;';	\
	    echo 'static int maui_osLen = 0;';			\
	) > $@
endif
	@ ( \
	    echo 'ifeq ($(strip $(CONFIG_MAUI_HAVE_BOOT) $(CONFIG_MAUI_BOOT_FILE)),$$(strip $$(CONFIG_MAUI_HAVE_BOOT) $$(CONFIG_MAUI_BOOT_FILE)))'; \
	    echo 'FILES_BOOT_UP_TO_DATE += $@'; \
	    echo 'endif' \
	) > .$@.boot



# Turtle Beach MultiSound

ifeq ($(CONFIG_MSNDCLAS_HAVE_BOOT),y)
    msnd_classic.o: msndperm.c msndinit.c

    msndperm.c: $(patsubst "%", %, $(CONFIG_MSNDCLAS_PERM_FILE)) bin2hex
	./bin2hex msndperm < $(CONFIG_MSNDCLAS_PERM_FILE) > $@
	@ ( \
	    echo 'ifeq ($(strip $(CONFIG_MSNDCLAS_HAVE_BOOT) $(CONFIG_MSNDCLAS_PERM_FILE)),$$(strip $$(CONFIG_MSNDCLAS_HAVE_BOOT) $$(CONFIG_MSNDCLAS_PERM_FILE)))'; \
	    echo 'FILES_BOOT_UP_TO_DATE += $@'; \
	    echo 'endif' \
	) > .$@.boot

    msndinit.c: $(patsubst "%", %, $(CONFIG_MSNDCLAS_INIT_FILE)) bin2hex
	./bin2hex msndinit < $(CONFIG_MSNDCLAS_INIT_FILE) > $@
	@ ( \
	    echo 'ifeq ($(strip $(CONFIG_MSNDCLAS_HAVE_BOOT) $(CONFIG_MSNDCLAS_INIT_FILE)),$$(strip $$(CONFIG_MSNDCLAS_HAVE_BOOT) $$(CONFIG_MSNDCLAS_INIT_FILE)))'; \
	    echo 'FILES_BOOT_UP_TO_DATE += $@'; \
	    echo 'endif' \
	) > .$@.boot
endif

ifeq ($(CONFIG_MSNDPIN_HAVE_BOOT),y)
    msnd_pinnacle.o: pndsperm.c pndspini.c

    pndsperm.c: $(patsubst "%", %, $(CONFIG_MSNDPIN_PERM_FILE)) bin2hex
	./bin2hex pndsperm < $(CONFIG_MSNDPIN_PERM_FILE) > $@
	@ ( \
	    echo 'ifeq ($(strip $(CONFIG_MSNDPIN_HAVE_BOOT) $(CONFIG_MSNDPIN_PERM_FILE)),$$(strip $$(CONFIG_MSNDPIN_HAVE_BOOT) $$(CONFIG_MSNDPIN_PERM_FILE)))'; \
	    echo 'FILES_BOOT_UP_TO_DATE += $@'; \
	    echo 'endif' \
	) > .$@.boot

    pndspini.c: $(patsubst "%", %, $(CONFIG_MSNDPIN_INIT_FILE)) bin2hex
	./bin2hex pndspini < $(CONFIG_MSNDPIN_INIT_FILE) > $@
	@ ( \
	    echo 'ifeq ($(strip $(CONFIG_MSNDPIN_HAVE_BOOT) $(CONFIG_MSNDPIN_INIT_FILE)),$$(strip $$(CONFIG_MSNDPIN_HAVE_BOOT) $$(CONFIG_MSNDPIN_INIT_FILE)))'; \
	    echo 'FILES_BOOT_UP_TO_DATE += $@'; \
	    echo 'endif' \
	) > .$@.boot
endif



# PSS (ECHO-ADI2111)

pss.o: pss_boot.h

ifeq ($(CONFIG_PSS_HAVE_BOOT),y)
    pss_boot.h: $(patsubst "%", %, $(CONFIG_PSS_BOOT_FILE)) bin2hex
	./bin2hex pss_synth < $(CONFIG_PSS_BOOT_FILE) > $@
else
    pss_boot.h:
	(							\
	    echo 'static unsigned char * pss_synth = NULL;';	\
	    echo 'static int pss_synthLen = 0;';		\
	) > $@
endif
	@ ( \
	    echo 'ifeq ($(strip $(CONFIG_PSS_HAVE_BOOT) $(CONFIG_PSS_BOOT_FILE)),$$(strip $$(CONFIG_PSS_HAVE_BOOT) $$(CONFIG_PSS_BOOT_FILE)))'; \
	    echo 'FILES_BOOT_UP_TO_DATE += $@'; \
	    echo 'endif' \
	) > .$@.boot



# MediaTrix AudioTrix Pro

trix.o: trix_boot.h

ifeq ($(CONFIG_TRIX_HAVE_BOOT),y)
    trix_boot.h: $(patsubst "%", %, $(CONFIG_TRIX_BOOT_FILE)) hex2hex
	./hex2hex -i trix_boot < $(CONFIG_TRIX_BOOT_FILE) > $@
else
    trix_boot.h:
	(							\
	    echo 'static unsigned char * trix_boot = NULL;';	\
	    echo 'static int trix_boot_len = 0;';		\
	) > $@
endif
	@ ( \
	    echo 'ifeq ($(strip $(CONFIG_TRIX_HAVE_BOOT) $(CONFIG_TRIX_BOOT_FILE)),$$(strip $$(CONFIG_TRIX_HAVE_BOOT) $$(CONFIG_TRIX_BOOT_FILE)))'; \
	    echo 'FILES_BOOT_UP_TO_DATE += $@'; \
	    echo 'endif' \
	) > .$@.boot



# Find boot files whose source file names have changed and force rebuild.

FILES_BOOT_UP_TO_DATE :=

FILES_BOOT_EXIST := $(wildcard .*.boot)
ifneq ($(FILES_BOOT_EXIST),)
include $(FILES_BOOT_EXIST)
endif

FILES_BOOT_CHANGED := $(strip \
    $(filter-out $(FILES_BOOT_UP_TO_DATE), \
	maui_boot.h pss_boot.h trix_boot.h))

ifneq ($(FILES_BOOT_CHANGED),)
$(FILES_BOOT_CHANGED): dummy
endif
