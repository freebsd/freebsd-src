# $FreeBSD$

all:
	@echo "make LINT only"

clean:
	rm LINT

LINT: ../../conf/NOTES NOTES makeLINT.sed
	cat ../../conf/NOTES NOTES | sed -E -n -f makeLINT.sed > LINT
