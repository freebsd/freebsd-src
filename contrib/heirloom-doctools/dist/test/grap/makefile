NAM=	grap

.SUFFIXES: .roff .more .ps

test:
	mv $(NAM).more $(NAM).more.orig
	make $(NAM).more
	diff $(NAM).more $(NAM).more.orig
	rm $(NAM).more
	mv $(NAM).more.orig $(NAM).more
	mv $(NAM).ps $(NAM).ps.orig
	make $(NAM).ps
	sed 's/.CreationDate.*/%/' $(NAM).ps > $(NAM).ps.sed
	diff $(NAM).ps.sed $(NAM).ps.orig
	rm $(NAM).ps $(NAM).ps.sed
	mv $(NAM).ps.orig $(NAM).ps

clean:
	rm -f *.more *.ps

.roff.more:
	grap $< | pic | neqn | nroff -Tlocale | col -x > $@

.roff.ps:
	grap $< | pic | eqn | troff | dpost > $@
