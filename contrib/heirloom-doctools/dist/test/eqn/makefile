.SUFFIXES: .roff .more .ps

test:
	mv eqn.more eqn.more.orig
	make eqn.more
	diff eqn.more eqn.more.orig
	rm eqn.more
	mv eqn.more.orig eqn.more
	mv eqn.ps eqn.ps.orig
	make eqn.ps
	sed 's/.CreationDate.*/%/' eqn.ps > eqn.ps.sed
	diff eqn.ps.sed eqn.ps.orig
	rm eqn.ps eqn.ps.sed
	mv eqn.ps.orig eqn.ps

clean:
	rm -f *.more *.ps

.roff.more:
	neqn $< | nroff -Tlocale | col -x > $@

.roff.ps:
	eqn $< | troff | dpost > $@
