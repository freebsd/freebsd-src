# $FreeBSD: src/sys/i386/conf/Makefile,v 1.7 2002/07/15 17:48:47 jhb Exp $

all:
	@echo "make LINT only"

clean:
	rm -f LINT

LINT: ../../conf/NOTES NOTES ../../conf/makeLINT.sed
	cat ../../conf/NOTES NOTES | sed -E -n -f ../../conf/makeLINT.sed > LINT
