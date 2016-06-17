BOOKS	:= wanbook.sgml z8530book.sgml mcabook.sgml videobook.sgml \
	   kernel-api.sgml parportbook.sgml kernel-hacking.sgml \
	   kernel-locking.sgml via-audio.sgml mousedrivers.sgml sis900.sgml \
	   deviceiobook.sgml procfs-guide.sgml tulip-user.sgml \
	   journal-api.sgml

PS	:=	$(patsubst %.sgml, %.ps, $(BOOKS))
PDF	:=	$(patsubst %.sgml, %.pdf, $(BOOKS))
HTML    :=      $(patsubst %.sgml, %, $(BOOKS))
IMG-parportbook := parport-share.fig parport-multi.fig parport-structure.fig
EPS-parportbook := $(patsubst %.fig, %.eps, $(IMG-parportbook))
PNG-parportbook := $(patsubst %.fig, %.png, $(IMG-parportbook))
C-procfs-example = procfs_example.sgml

books:	$(BOOKS)

$(BOOKS): $(TOPDIR)/scripts/docproc

.PHONY:	books ps pdf html clean mrproper

ps:	$(PS)

pdf:	$(PDF)

html:	$(HTML)

man:	kernel-api-man

%.eps: %.fig
	fig2dev -Leps $< $@

%.png: %.fig
	fig2dev -Lpng $< $@

%.sgml: %.c
	echo "<programlisting>" > $@
	expand --tabs=8 < $< | \
	sed -e "s/&/\\&amp;/g" \
	    -e "s/</\\&lt;/g" \
	    -e "s/>/\\&gt;/g" >> $@
	echo "</programlisting>" >> $@


$(TOPDIR)/scripts/docproc:
	$(MAKE) -C $(TOPDIR)/scripts docproc

mousedrivers.sgml: mousedrivers.tmpl
	$(TOPDIR)/scripts/docgen <$< >$@

kernel-hacking.sgml: kernel-hacking.tmpl
	$(TOPDIR)/scripts/docgen <$< >$@

kernel-locking.sgml: kernel-locking.tmpl
	$(TOPDIR)/scripts/docgen <$< >$@

wanbook.sgml: wanbook.tmpl $(TOPDIR)/drivers/net/wan/syncppp.c
	$(TOPDIR)/scripts/docgen $(TOPDIR)/drivers/net/wan/syncppp.c \
		<wanbook.tmpl >wanbook.sgml

z8530book.sgml: z8530book.tmpl $(TOPDIR)/drivers/net/wan/z85230.c
	$(TOPDIR)/scripts/docgen $(TOPDIR)/drivers/net/wan/z85230.c \
		<z8530book.tmpl >z8530book.sgml

via-audio.sgml: via-audio.tmpl $(TOPDIR)/drivers/sound/via82cxxx_audio.c
	$(TOPDIR)/scripts/docgen $(TOPDIR)/drivers/sound/via82cxxx_audio.c \
		<via-audio.tmpl >via-audio.sgml

tulip-user.sgml: tulip-user.tmpl
	$(TOPDIR)/scripts/docgen <$< >$@

sis900.sgml: sis900.tmpl $(TOPDIR)/drivers/net/sis900.c
	$(TOPDIR)/scripts/docgen $(TOPDIR)/drivers/net/sis900.c \
		<sis900.tmpl >sis900.sgml

deviceiobook.sgml: deviceiobook.tmpl
	$(TOPDIR)/scripts/docgen <deviceiobook.tmpl >deviceiobook.sgml

mcabook.sgml: mcabook.tmpl $(TOPDIR)/arch/i386/kernel/mca.c
	$(TOPDIR)/scripts/docgen $(TOPDIR)/arch/i386/kernel/mca.c \
		<mcabook.tmpl >mcabook.sgml

videobook.sgml: videobook.tmpl $(TOPDIR)/drivers/media/video/videodev.c
	$(TOPDIR)/scripts/docgen $(TOPDIR)/drivers/media/video/videodev.c \
		<videobook.tmpl >videobook.sgml

procfs-guide.sgml:  procfs-guide.tmpl procfs_example.sgml
	$(TOPDIR)/scripts/docgen < procfs-guide.tmpl >$@

APISOURCES :=	$(TOPDIR)/drivers/media/video/videodev.c \
		$(TOPDIR)/arch/i386/kernel/irq.c \
		$(TOPDIR)/arch/i386/kernel/mca.c \
		$(TOPDIR)/arch/i386/kernel/mtrr.c \
		$(TOPDIR)/drivers/char/misc.c \
		$(TOPDIR)/kernel/printk.c \
		$(TOPDIR)/drivers/net/net_init.c \
		$(TOPDIR)/drivers/net/8390.c \
		$(TOPDIR)/drivers/char/serial.c \
		$(TOPDIR)/drivers/pci/pci.c \
		$(TOPDIR)/drivers/hotplug/pci_hotplug_core.c \
		$(TOPDIR)/drivers/hotplug/pci_hotplug_util.c \
		$(TOPDIR)/drivers/block/ll_rw_blk.c \
		$(TOPDIR)/drivers/sound/sound_core.c \
		$(TOPDIR)/drivers/sound/sound_firmware.c \
		$(TOPDIR)/drivers/net/wan/syncppp.c \
		$(TOPDIR)/drivers/net/wan/z85230.c \
		$(TOPDIR)/drivers/usb/usb.c \
		$(TOPDIR)/drivers/video/fbmem.c \
		$(TOPDIR)/drivers/video/fbcmap.c \
		$(TOPDIR)/drivers/video/fbcon.c \
		$(TOPDIR)/drivers/video/fbgen.c \
		$(TOPDIR)/drivers/video/fonts.c \
		$(TOPDIR)/drivers/video/macmodes.c \
		$(TOPDIR)/drivers/video/modedb.c \
		$(TOPDIR)/fs/devfs/base.c \
		$(TOPDIR)/fs/locks.c \
		$(TOPDIR)/include/asm-i386/bitops.h \
		$(TOPDIR)/kernel/pm.c \
		$(TOPDIR)/kernel/ksyms.c \
		$(TOPDIR)/kernel/kmod.c \
		$(TOPDIR)/kernel/module.c \
		$(TOPDIR)/kernel/printk.c \
		$(TOPDIR)/kernel/sched.c \
		$(TOPDIR)/kernel/sysctl.c \
		$(TOPDIR)/lib/string.c \
		$(TOPDIR)/lib/vsprintf.c \
		$(TOPDIR)/net/netsyms.c
 
kernel-api.sgml: kernel-api.tmpl $(APISOURCES)
	$(TOPDIR)/scripts/docgen $(APISOURCES) \
		<kernel-api.tmpl >kernel-api.sgml

kernel-api-man: $(APISOURCES)
	@rm -rf $(TOPDIR)/Documentation/man
	$(TOPDIR)/scripts/kernel-doc -man $^ | \
		$(PERL) $(TOPDIR)/scripts/split-man $(TOPDIR)/Documentation/man

parportbook parportbook.pdf: $(PNG-parportbook)
parportbook.ps: $(EPS-parportbook)
parportbook.sgml: parportbook.tmpl $(TOPDIR)/drivers/parport/init.c
	$(TOPDIR)/scripts/docgen $(TOPDIR)/drivers/parport/init.c <$< >$@


JBDSOURCES :=  $(TOPDIR)/include/linux/jbd.h \
	       $(TOPDIR)/fs/jbd/journal.c \
	       $(TOPDIR)/fs/jbd/recovery.c \
	       $(TOPDIR)/fs/jbd/transaction.c

journal-api.sgml: journal-api.tmpl $(JBDSOURCES)
	$(TOPDIR)/scripts/docgen   $(JBDSOURCES) \
		<journal-api.tmpl >journal-api.sgml


DVI	:=	$(patsubst %.sgml, %.dvi, $(BOOKS))
AUX	:=	$(patsubst %.sgml, %.aux, $(BOOKS))
TEX	:=	$(patsubst %.sgml, %.tex, $(BOOKS))
LOG	:=	$(patsubst %.sgml, %.log, $(BOOKS))
OUT	:=	$(patsubst %.sgml, %.out, $(BOOKS))

clean:
	rm -f core *~
	rm -f $(BOOKS)
	rm -f $(DVI) $(AUX) $(TEX) $(LOG) $(OUT)
	rm -f $(PNG-parportbook) $(EPS-parportbook)
	rm -f $(C-procfs-example)

mrproper: clean
	rm -f $(PS) $(PDF)
	rm -f -r $(HTML)
	rm -f .depend
	rm -f $(TOPDIR)/scripts/mkdep-docbook
	rm -rf DBTOHTML_OUTPUT*

%.ps : %.sgml
	@(which db2ps > /dev/null 2>&1) || \
	 (echo "*** You need to install DocBook stylesheets ***"; \
	  exit 1)
	db2ps $<

%.pdf : %.sgml
	@(which db2pdf > /dev/null 2>&1) || \
	 (echo "*** You need to install DocBook stylesheets ***"; \
	  exit 1)
	db2pdf $<

%:	%.sgml
	@(which db2html > /dev/null 2>&1) || \
	 (echo "*** You need to install DocBook stylesheets ***"; \
	  exit 1)
	rm -rf $@
	db2html $<
	if [ ! -z "$(PNG-$@)" ]; then cp $(PNG-$@) $@; fi

#
# we could have our own dependency generator
#
#
# .depend: $(TOPDIR)/scripts/mkdep-docbook
#	$(TOPDIR)/scripts/mkdep-docbook $(wildcard *.tmpl) > .depend

include $(TOPDIR)/Rules.make

