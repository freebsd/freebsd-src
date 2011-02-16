# $FreeBSD: src/tools/regression/lib/libc/regex/Makefile,v 1.1.30.1.6.1 2010/12/21 17:09:25 kensmith Exp $

GROT=	../../../../../lib/libc/regex/grot

.PHONY: tests
tests:
	cd ${GROT}; make && echo "PASS regex"

.PHONY: clean
clean:
	cd ${GROT}; make spotless
