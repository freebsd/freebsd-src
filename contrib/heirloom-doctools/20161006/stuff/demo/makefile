.SUFFIXES: .tr .ps .pdf

# You need at least Ghostscript 8.53 to create PDF/X documents.
DISTFLAGS = -q -dBATCH -dNOPAUSE -sDEVICE=pdfwrite \
	-sProcessColorModel=DeviceGray -dPDFX

.tr.ps:
	troff $(TFLAGS) $< | dpost >$@

.ps.pdf:
	gs -sOutputFile=$@ $(DISTFLAGS) -f PDFX_def.ps -f $<

all: troffdemo.ps troffdemo_otf.ps troffdemo.pdf troffdemo_otf.pdf

troffdemo_otf.ps: troffdemo.tr troffdemo_otf.tr

clean:
	rm -f troffdemo.pdf troffdemo.ps troffdemo_otf.ps \
		troffdemo_otf.pdf core log *~
