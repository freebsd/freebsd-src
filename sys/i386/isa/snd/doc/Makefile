# Makefile for the sound documentation

ARTICLE=sound
INPUTS=
STYLES=
.SUFFIXES: .fig .ps
OTHERSTUFF=
FIGURES=
PSS=
ALLFIG= $(FIGURES)
COMPAT= psfigps.tex

ALLPS= $(ALLFIG:.fig=.ps)

ALLSRCS= $(ARTICLE).tex $(STYLES) $(ALLFIG) $(PSS) $(INPUTS) \
	$(COMPAT) $(OTHERSTUFF) Makefile

.fig.ps:
	fig2dev -L ps -m 0.5 $*.fig > $*.ps

all: $(ARTICLE).dvi pnp.dvi ioctl.dvi Makefile

$(ARTICLE).dvi: allfig $(ARTICLE).tex $(INPUTS) $(STYLES) $(PSS)
	latex $(ARTICLE).tex

pnp.dvi: allfig pnp.tex $(INPUTS) $(STYLES) $(PSS)
	latex pnp.tex

ioctl.dvi: allfig pnp.tex $(INPUTS) $(STYLES) $(PSS)
	latex ioctl.tex

allfig: $(ALLPS)

clean:
	rm -f $(ALLPS) a c *.dvi *.aux *.log rl

tgz: $(ALLSRCS)
	tar cvf - $(ALLSRCS) | gzip -9 > $(ARTICLE).tgz


