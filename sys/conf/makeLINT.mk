# $FreeBSD: src/sys/conf/makeLINT.mk,v 1.1.36.1 2010/02/10 00:26:20 kensmith Exp $

all:
	@echo "make LINT only"

clean:
	rm -f LINT

NOTES=	../../conf/NOTES NOTES
LINT: ${NOTES} ../../conf/makeLINT.sed
	cat ${NOTES} | sed -E -n -f ../../conf/makeLINT.sed > ${.TARGET}
