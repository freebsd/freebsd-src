# $FreeBSD$

all:
	@echo "make LINT only"

clean:
	rm -f LINT

LINT: ../../conf/NOTES NOTES ../../conf/makeLINT.sed
	cat ../../conf/NOTES NOTES | sed -E -n -f ../../conf/makeLINT.sed > LINT
