# $FreeBSD: src/tools/regression/lib/libc/regex/Makefile,v 1.1.34.1 2010/12/21 17:10:29 kensmith Exp $

GROT=	../../../../../lib/libc/regex/grot

.PHONY: tests
tests:
	cd ${GROT}; make && echo "PASS regex"

.PHONY: clean
clean:
	cd ${GROT}; make spotless
