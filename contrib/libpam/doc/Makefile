
### $Id: Makefile,v 1.3 2001/01/22 08:03:01 agmorgan Exp $

include ../Make.Rules

# These two should probably be moved into autoconf...
DOCDIR=/usr/doc/Linux-PAM
MANDIR=/usr/man

#######################################################

FILES=pam pam_appl pam_modules
FSRCS=pam.sgml pam_appl.sgml pam_modules.sgml

TEXTS=txts/pam.txt txts/pam_appl.txt txts/pam_modules.txt
HTMLS=html/pam.html html/pam_appl.html html/pam_modules.html
PSFILES=ps/pam.ps ps/pam_appl.ps ps/pam_modules.ps

MODULES=$(shell ls modules/*.sgml)

#######################################################

dummy:
	@echo "Making the documentation..."
	@make all

all: htmls texts postscript

htmls: $(HTMLS)

$(HTMLS) : $(FSRCS)
ifeq ($(HAVE_SGML2HTML),yes)
	@for i in $(FILES) ; do \
	if [ ! -f "html/$$i.html" ] || [ "$$i.sgml" -nt "html/$$i.html" ]; \
	then \
		cd html ; sgml2html ../$$i ; \
		if [ $$? -ne 0 ]; then exit 1 ; fi ; \
		cd .. ; \
	fi ; \
	done
else
	@echo XXX - you do not have the sgml2html binary installed
endif

texts: $(TEXTS)

$(TEXTS) : $(FSRCS)
ifeq ($(HAVE_SGML2TXT),yes)
	@for i in $(FILES) ; do \
		if [ ! -f "txts/$$i.txt" ] \
				|| [ "$$i.sgml" -nt "txts/$$i.txt" ]; then \
			cd txts ; sgml2txt ../$$i ; cd .. ; \
		fi ; \
	done
else
	@echo XXX - you do not have the sgml2txt binary installed
endif

postscript: $(PSFILES)

$(PSFILES): $(FSRCS)
ifneq ($(PSER),)
	@for i in $(FILES) ; do \
	if [ ! -f "ps/$$i.ps" ] || [ "$$i.sgml" -nt "ps/$$i.ps" ]; then \
		cd ps ; $(PSER) ../$$i ; cd .. ; \
	fi ; \
	done
else
	@echo XXX - neither sgml2ps nor sgml2latex binaries are installed
endif

pam.sgml: pam_source.sgml MODULES-SGML CREDITS
	@sed -e '/^<!\-\- insert\-file MODULES\-SGML \-\->/r MODULES-SGML' pam_source.sgml | sed -e '/^<!\-\- insert\-file CREDITS \-\->/r CREDITS' > pam.sgml

MODULES-SGML: $(MODULES)
	@echo 'Building module text from files in modules/*.sgml'
	@rm -f MODULES-SGML
	@echo '<!-- modules included:' > MODULES-SGML
	@ls modules/*.sgml >> MODULES-SGML
	@echo '  and that is all -->' >> MODULES-SGML
	@cat modules/*.sgml >> MODULES-SGML

extraclean: clean

remove:
	cd man && for file in *.3 ; do \
	  rm -f $(FAKEROOT)$(MANDIR)/man3/$$file ; \
	done
	cd man && for file in *.8 ; do \
	  rm -f $(FAKEROOT)$(MANDIR)/man8/$$file ; \
	done
	cd txts && for file in *.txt; do \
	  rm -f $(FAKEROOT)$(DOCDIR)/text/$$file ; \
	done
	cd ps && for file in *.ps; do \
	  rm -f $(FAKEROOT)$(DOCDIR)/ps/$$file  ; \
	done
	cd html && for file in *.html; do \
	  rm -f $(FAKEROOT)$(DOCDIR)/html/$$file ; \
	done

install: all
ifeq ($(HAVE_SGML2TXT),yes)
	mkdir -p $(FAKEROOT)$(DOCDIR)/text
	for file in txts/*.txt; do \
	  install -m 644 $$file $(FAKEROOT)$(DOCDIR)/text ; \
	done
endif
ifneq ($(PSER),)
	mkdir -p $(FAKEROOT)$(DOCDIR)/ps
	for file in ps/*.ps; do \
	  install -m 644 $$file $(FAKEROOT)$(DOCDIR)/ps  ; \
	done
endif
ifeq ($(HAVE_SGML2HTML),yes)
	mkdir -p $(FAKEROOT)$(DOCDIR)/html
	for file in html/*.html; do \
	  install -m 644 $$file $(FAKEROOT)$(DOCDIR)/html ; \
	done
endif
	mkdir -p $(FAKEROOT)$(MANDIR)/man{3,8}
	for file in man/*.3 ; do \
	  install -m 644 $$file $(FAKEROOT)$(MANDIR)/man3 ; \
	done
	for file in man/*.8 ; do \
	  install -m 644 $$file $(FAKEROOT)$(MANDIR)/man8 ; \
	done

spec:
	cd specs/formatter && make
	specs/formatter/padout < specs/draft-morgan-pam.raw > specs/draft-morgan-pam-current.txt

releasedocs: all spec
	tar zvfc Linux-PAM-$(MAJOR_REL).$(MINOR_REL)-docs.tar.gz --exclude CVS  html ps txts specs/draft-morgan-pam-current.txt

clean:
	rm -f *~ *.bak
	rm -f html/pam*.html
	rm -f man/*~
	rm -f $(TEXTS)
	rm -f $(PSFILES) ps/missfont.log
	rm -f MODULES-SGML pam.sgml
	rm -f specs/draft-morgan-pam-current.txt
	make -C specs/formatter clean

