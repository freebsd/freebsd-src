# $FreeBSD$

all:
	@echo "make LINT only"

clean:
	rm LINT

LINT: NOTES makeLINT.pl
	perl5 makeLINT.pl < NOTES > LINT
