# This makefile for Readline library documentation is in -*- text -*- mode.
# Emacs likes it that way.
TEXI2DVI = texi2dvi

RM = rm -f

INSTALL_DATA = cp
infodir = /usr/local/info

RLSRC = rlman.texinfo rluser.texinfo rltech.texinfo
HISTSRC = hist.texinfo hsuser.texinfo hstech.texinfo

DVIOBJ = readline.dvi history.dvi
INFOOBJ = readline.info history.info
PSOBJ = readline.ps history.ps
HTMLOBJ = readline.html history.html

all: info dvi html

readline.dvi: $(RLSRC)
	$(TEXI2DVI) rlman.texinfo
	mv rlman.dvi readline.dvi

readline.info: $(RLSRC)
	makeinfo --no-split -o $@ rlman.texinfo

history.dvi: ${HISTSRC}
	$(TEXI2DVI) hist.texinfo
	mv hist.dvi history.dvi

history.info: ${HISTSRC}
	makeinfo --no-split -o $@ hist.texinfo

readline.ps:	readline.dvi
	dvips -D 300 -o $@ readline.dvi

history.ps:	history.dvi
	dvips -D 300 -o $@ history.dvi

readline.html:	${RLSRC}
	texi2html rlman.texinfo
	sed -e 's:rlman.html:readline.html:' -e 's:rlman_toc.html:readline_toc.html:' rlman.html > readline.html
	sed -e 's:rlman.html:readline.html:' -e 's:rlman_toc.html:readline_toc.html:' rlman_toc.html > readline_toc.html
	rm -f rlman.html rlman_toc.html

history.html:	${HISTSRC}
	texi2html hist.texinfo
	sed -e 's:hist.html:history.html:' -e 's:hist_toc.html:history_toc.html:' hist.html > history.html
	sed -e 's:hist.html:history.html:' -e 's:hist_toc.html:history_toc.html:' hist_toc.html > history_toc.html
	rm -f hist.html hist_toc.html

info:	$(INFOOBJ)
dvi:	$(DVIOBJ)
ps:	$(PSOBJ)
html:	$(HTMLOBJ)

clean:
	$(RM) *.aux *.cp *.fn *.ky *.log *.pg *.toc *.tp *.vr *.cps *.pgs \
	      *.fns *.kys *.tps *.vrs *.o core

distclean: clean
mostlyclean: clean

maintainer-clean: clean
	$(RM) *.dvi *.info *.info-* *.ps *.html

install:	info
	${INSTALL_DATA} readline.info $(infodir)/readline.info
	${INSTALL_DATA} history.info $(infodir)/history.info
