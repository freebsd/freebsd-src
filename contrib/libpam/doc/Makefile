
### $Id: Makefile,v 1.9 1997/01/04 21:55:52 morgan Exp $
### $FreeBSD$

TXTER=sgml2txt
HTMLER=sgml2html
# older distributions use, sgml2ps
PSER=sgml2latex -p

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
	@for i in $(FILES) ; do \
	if [ ! -f "html/$$i.html" ] || [ "$$i.sgml" -nt "html/$$i.html" ]; \
	then \
		cd html ; $(HTMLER) ../$$i ; \
		if [ $$? -ne 0 ]; then exit 1 ; fi ; \
		cd .. ; \
	fi ; \
	done

texts: $(TEXTS)

$(TEXTS) : $(FSRCS)
	@for i in $(FILES) ; do \
		if [ ! -f "txts/$$i.txt" ] \
				|| [ "$$i.sgml" -nt "txts/$$i.txt" ]; then \
			cd txts ; $(TXTER) ../$$i ; cd .. ; \
		fi ; \
	done

postscript: $(PSFILES)

$(PSFILES): $(FSRCS)
	@for i in $(FILES) ; do \
	if [ ! -f "ps/$$i.ps" ] || [ "$$i.sgml" -nt "ps/$$i.ps" ]; then \
		cd ps ; $(PSER) ../$$i ; cd .. ; \
	fi ; \
	done

pam.sgml: pam_source.sgml MODULES-SGML
	@sed -e '/^<!\-\- insert\-file MODULES\-SGML \-\->/r MODULES-SGML' pam_source.sgml > pam.sgml

MODULES-SGML: $(MODULES)
	@echo 'Building module text from files in modules/*.sgml'
	@rm -f MODULES-SGML
	@echo '<!-- modules included:' > MODULES-SGML
	@ls modules/*.sgml >> MODULES-SGML
	@echo '  and that is all -->' >> MODULES-SGML
	@cat modules/*.sgml >> MODULES-SGML

extraclean: clean

clean:
	rm -f *~ *.bak
	rm -f html/pam*.html
	rm -f man/*~
	rm -f $(TEXTS)
	rm -f $(PSFILES)
	rm -f MODULES-SGML pam.sgml

