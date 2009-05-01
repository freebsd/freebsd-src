# $FreeBSD: src/sys/conf/makeLINT.mk,v 1.1.32.1 2009/04/15 03:14:26 kensmith Exp $

all:
	@echo "make LINT only"

clean:
	rm -f LINT

NOTES=	../../conf/NOTES NOTES
LINT: ${NOTES} ../../conf/makeLINT.sed
	cat ${NOTES} | sed -E -n -f ../../conf/makeLINT.sed > ${.TARGET}
