# $FreeBSD$

all:
	rm -f *.uu
	for file in *.bin; do \
	  uuencode -o $$file.uu $$file $$file; \
	  rm $$file; \
	done
