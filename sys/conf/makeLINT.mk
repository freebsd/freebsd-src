# $FreeBSD: src/sys/conf/makeLINT.mk,v 1.1 2003/02/26 23:36:58 ru Exp $

all:
	@echo "make LINT only"

clean:
	rm -f LINT

NOTES=	../../conf/NOTES NOTES
LINT: ${NOTES} ../../conf/makeLINT.sed
	cat ${NOTES} | sed -E -n -f ../../conf/makeLINT.sed > ${.TARGET}
