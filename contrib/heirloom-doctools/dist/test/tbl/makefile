.SUFFIXES: .roff .doc .more .ps

test:
	# tbl.more
	mv tbl.more tbl.more.orig
	make tbl.more
	#diff tbl.more.orig tbl.more
	rm tbl.more
	mv tbl.more.orig tbl.more
	# tbl.ps
	mv tbl.ps tbl.ps.orig
	make tbl.ps
	sed 's/.CreationDate.*/%/' tbl.ps > tbl.ps.sed
	diff tbl.ps.orig tbl.ps.sed
	rm tbl.ps tbl.ps.sed
	mv tbl.ps.orig tbl.ps
	# tbl.doc.more
	mv tbl.doc.more tbl.doc.more.orig
	make tbl.doc.more
	diff tbl.doc.more.orig tbl.doc.more
	rm tbl.doc.more
	mv tbl.doc.more.orig tbl.doc.more
	# tbl.doc.ps
	mv tbl.doc.ps tbl.doc.ps.orig
	make tbl.doc.ps
	sed 's/.CreationDate.*/%/' tbl.doc.ps > tbl.doc.ps.sed
	diff tbl.doc.ps.orig tbl.doc.ps.sed
	rm tbl.doc.ps tbl.doc.ps.sed
	mv tbl.doc.ps.orig tbl.doc.ps

clean:
	rm -f *.more *.ps

.roff.more:
	soelim $< | tbl -Tlocale | neqn | nroff -Tlocale | col -x > $@

.roff.ps:
	soelim $< | tbl -g | eqn | troff | dpost > $@

tbl.doc.more: tbl.doc
	tbl -Tlocale ND.roff tbl.doc | neqn | nroff -Tlocale -ms | col -x > $@

tbl.doc.ps: tbl.doc
	tbl -g tbl.doc | eqn | troff -ms | dpost > $@
