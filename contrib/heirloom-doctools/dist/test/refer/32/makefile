test:
	mv out out.orig
	printf '.[\nopenbsd\n.]\n' | refer -p bib > out
	diff out out.orig
	rm out *.ig
	mv out.orig out
