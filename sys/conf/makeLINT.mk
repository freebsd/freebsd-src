# $FreeBSD: src/sys/conf/makeLINT.mk,v 1.1.30.1 2008/11/25 02:59:29 kensmith Exp $

all:
	@echo "make LINT only"

clean:
	rm -f LINT

NOTES=	../../conf/NOTES NOTES
LINT: ${NOTES} ../../conf/makeLINT.sed
	cat ${NOTES} | sed -E -n -f ../../conf/makeLINT.sed > ${.TARGET}
