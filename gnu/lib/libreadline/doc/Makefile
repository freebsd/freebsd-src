# This makefile for History library documentation is in -*- text -*- mode.
# Emacs likes it that way.

INFOBJ = readline.info history.info

all: info

readline.info: rlman.texinfo rluser.texinfo rltech.texinfo
	makeinfo rlman.texinfo

history.info: hist.texinfo hsuser.texinfo hstech.texinfo
	makeinfo hist.texinfo

info:	$(INFOOBJ)

clean:
	rm -f *.aux *.cp *.fn *.ky *.log *.pg *.toc *.tp *.vr *.cps *.pgs \
	      *.fns *.kys *.tps *.vrs *.o core

squeaky-clean:
	rm -f *.aux *.cp *.fn *.ky *.log *.pg *.toc *.tp *.vr *.cps *.pgs \
	      *.info *.info-* *.fns *.kys *.tps *.vrs *.o core

distclean: clean
realclean: clean
