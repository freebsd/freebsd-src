# This makefile for History library documentation is in -*- text -*- mode.
# Emacs likes it that way.

DOC_SUPPORT = ../../doc-support/
TEXINDEX    = $(DOC_SUPPORT)/texindex

TEX    = tex
DVIOBJ = history.dvi
INFOBJ = history.info

all: $(DVIOBJ) $(INFOBJ)

history.dvi: hist.texinfo hsuser.texinfo hstech.texinfo
	$(TEX) hist.texinfo
	$(TEXINDEX) hist.??
	$(TEX) hist.texinfo
	mv hist.dvi history.dvi

history.info: hist.texinfo hsuser.texinfo hstech.texinfo
	makeinfo hist.texinfo

./texinfo.tex: $(DOC_SUPPORT)texinfo.tex
	ln -s $(DOC_SUPPORT)texinfo.tex .

$(TEXINDEX):
	(cd $(DOC_SUPPORT); $(MAKE) $(MFLAGS) CFLAGS='$(CFLAGS)' texindex)

clean:
	rm -f *.aux *.cp *.fn *.ky *.log *.pg *.toc *.tp *.vr *.cps *.pgs \
	      *.fns *.kys *.tps *.vrs *.o core texinfo.tex

squeaky-clean:
	rm -f *.aux *.cp *.fn *.ky *.log *.pg *.toc *.tp *.vr *.cps *.pgs \
	      *.dvi *.info *.info-* *.fns *.kys *.tps *.vrs *.o core
