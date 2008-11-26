# $FreeBSD: src/sys/conf/makeLINT.mk,v 1.1.28.1 2008/10/02 02:57:24 kensmith Exp $

all:
	@echo "make LINT only"

clean:
	rm -f LINT

NOTES=	../../conf/NOTES NOTES
LINT: ${NOTES} ../../conf/makeLINT.sed
	cat ${NOTES} | sed -E -n -f ../../conf/makeLINT.sed > ${.TARGET}
