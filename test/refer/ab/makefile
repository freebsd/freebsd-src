test:
	mv out out.orig
	refer -p ab.bib ab.ms | nroff -Tlocale -ms | \
	    sed '/^[[:space:]]*[A-Z]/d' > out
	diff out out.orig
	rm out *.ig
	mv out.orig out
