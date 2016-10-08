DISTFLAGS = -q -dBATCH -dNOPAUSE -sDEVICE=pdfwrite \
	-sProcessColorModel=DeviceGray -dPDFSETTINGS=/prepress
CRYPT = -sOwnerPassword="`dd if=/dev/random bs=12 count=1 2>/dev/null | \
	md5sum | awk '{ print $$1 }'` " -dEncryptionR=3 \
	-dKeyLength=128 -dPermissions=-300
TROFFFLAGS = -mgchar

.SUFFIXES: .tr .ps .pdf .html .more

.tr.ps:
	rm -f $@; trap "rm -f $$$$.ps" 0 2 3; \
		tbl -g $< | troff $(TROFFFLAGS) | \
		    dpost >$$$$.ps && mv $$$$.ps $@

.ps.pdf:
	gs -sOutputFile=$@ $(DISTFLAGS) -f $<

.tr.html:
	tbl $< | troff -Thtml $(TROFFFLAGS) | dhtml -t \
	    "Nroff/Troff User's Manual" > $@

.tr.more:
	tbl -Tlocale $< | nroff -Tlocale | col -x > $@

all: doc.ps

test:
	[ -e doc.more ] && mv doc.more doc.more.orig
	make doc.more
	sed '/dotlessj/d' doc.more >doc.more.sed
	diff -d doc.more.orig doc.more.sed
	rm doc.more.sed doc.more
	mv doc.more.orig doc.more
	[ -e doc.ps ] && mv doc.ps doc.ps.orig
	make doc.ps
	sed 's/.CreationDate.*/%/' doc.ps >doc.ps.sed
	diff -d doc.ps.orig doc.ps.sed
	rm doc.ps.sed doc.ps
	mv doc.ps.orig doc.ps

clean:
	rm -f doc.ps doc.pdf doc.html doc.more core log *~

mrproper: clean
