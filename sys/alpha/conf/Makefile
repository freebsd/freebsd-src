# $FreeBSD: src/sys/alpha/conf/Makefile,v 1.1 2002/07/15 17:50:17 jhb Exp $

all:
	@echo "make LINT only"

clean:
	rm -f LINT

LINT: ../../conf/NOTES NOTES ../../conf/makeLINT.sed
	cat ../../conf/NOTES NOTES | sed -E -n -f ../../conf/makeLINT.sed > LINT
