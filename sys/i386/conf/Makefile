# $FreeBSD$

all:
	@echo "make LINT only"

clean:
	rm LINT

LINT: ../../conf/NOTES NOTES makeLINT.pl
	cat ../../conf/NOTES NOTES | perl5 makeLINT.pl > LINT
