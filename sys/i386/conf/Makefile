# $FreeBSD$

all:
	@echo "make LINT only"

clean:
	rm LINT

LINT: ../../conf/NOTES NOTES makeLINT.sh
	cat ../../conf/NOTES NOTES | sh makeLINT.sh > LINT
