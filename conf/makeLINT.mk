# $FreeBSD$

all:
	@echo "make LINT only"

clean:
	rm -f LINT

NOTES=	../../conf/NOTES NOTES
LINT: ${NOTES} ../../conf/makeLINT.sed
	cat ${NOTES} | sed -E -n -f ../../conf/makeLINT.sed > ${.TARGET}
