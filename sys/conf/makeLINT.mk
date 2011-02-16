# $FreeBSD: src/sys/conf/makeLINT.mk,v 1.1.38.1 2010/12/21 17:10:29 kensmith Exp $

all:
	@echo "make LINT only"

clean:
	rm -f LINT

NOTES=	../../conf/NOTES NOTES
LINT: ${NOTES} ../../conf/makeLINT.sed
	cat ${NOTES} | sed -E -n -f ../../conf/makeLINT.sed > ${.TARGET}
