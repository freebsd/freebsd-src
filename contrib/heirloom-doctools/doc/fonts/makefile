DISTFLAGS = -q -dBATCH -dNOPAUSE -sDEVICE=pdfwrite \
	-sProcessColorModel=DeviceGray -dPDFSETTINGS=/prepress
CRYPT = -sOwnerPassword="`dd if=/dev/random bs=12 count=1 2>/dev/null | \
	md5sum | awk '{ print $$1 }'` " -dEncryptionR=3 \
	-dKeyLength=128 -dPermissions=-300

.SUFFIXES: .tr .ps .pdf .more

.tr.ps:
	rm -f $@; trap "rm -f $$$$.ps" 0 2 3; \
		tbl $< | eqn | troff | dpost -e4 >$$$$.ps && mv $$$$.ps $@

.ps.pdf:
	gs -sOutputFile=$@ $(DISTFLAGS) $(CRYPT) -f $<

.tr.more:
	tbl -Tlocale $< | nroff -Tlocale | col -x > $@

all: doc.ps

print.ps: doc.pdf
	pdftops -paper match doc.pdf $@

book.ps: print.ps
	psbook print.ps | psnup -2 -pa4 -Pa5 | \
	sed 's/^\(%%DocumentMedia:\).*/\1 x595y842 595 842 0 () ()/' >$@

odd.ps: book.ps
	psselect -o book.ps $@

even.ps: book.ps
	psselect -e book.ps $@

test:
	[ -e doc.more ] && mv doc.more doc.more.orig
	make doc.more
	diff -d doc.more.orig doc.more
	mv doc.more.orig doc.more
	[ -e doc.ps ] && mv doc.ps doc.ps.orig
	make doc.ps
	rm fonts.tmac
	sed 's/.CreationDate.*/%/' doc.ps >doc.ps.sed
	diff -d doc.ps.orig doc.ps.sed
	rm doc.ps.sed
	mv doc.ps.orig doc.ps

clean:
	rm -f book.ps book.pdf doc.ps doc.pdf print.ps doc.more \
		odd.ps odd.pdf even.ps even.pdf core log *~ fonts.tmac

fonts.tmac:
	@[ -n "$$TROFFONTS" ] || \
	    (echo "Error: Environment variable TROFFONTS not set" && false)
	@for F in GaramondPremrPro GillSansStd LinLibertine ; do \
		find $$(echo $$TROFFONTS | sed -e 's/:/ /g') -name "$$F*" | \
		    grep $$F > /dev/null && echo ".nr $$F 1" >> $@ || true; \
	done
	@grep nr $@ > /dev/null || \
	    (echo 'Error: None of the fonts "LinLibertine", ' \
	     '"GaramondPremrPro" and "GillSansStd" found' && false)

mrproper: clean

doc.ps: fonts.tmac
