# This makefile for Readline library documentation is in -*- text -*- mode.
# Emacs likes it that way.

DVIOBJ = readline.dvi history.dvi
INFOBJ = readline.info history.info

all: $(DVIOBJ) $(INFOBJ)

readline.dvi: rlman.texinfo rluser.texinfo rltech.texinfo
	tex rlman.texinfo
	tex rlman.texinfo
	mv rlman.dvi readline.dvi

history.dvi: hist.texinfo hsuser.texinfo hstech.texinfo
	tex hist.texinfo
	tex hist.texinfo
	mv hist.dvi history.dvi

readline.info: rlman.texinfo rluser.texinfo rltech.texinfo
	makeinfo rlman.texinfo

history.info: hist.texinfo hsuser.texinfo hstech.texinfo
	makeinfo hist.texinfo

clean:
	rm -f *.log *.cp *.ky *.tp *.vr *.fn *.aux *.pg *.toc core

squeaky-clean:
	rm -f *.log *.cp *.ky *.tp *.vr *.fn *.aux *.pg *.toc core *.dvi *.~* *.info

