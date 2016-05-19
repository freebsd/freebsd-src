# $Id$

TCLIST?=	tclist

.PHONY: all

all: tc

tc: ${TCLIST}
	${.CURDIR}/../common/gen.awk ${.ALLSRC} > ${.TARGET}
	chmod +x ${.TARGET}

clean:
	rm -rf tc

