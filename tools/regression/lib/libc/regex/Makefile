# $FreeBSD: src/tools/regression/lib/libc/regex/Makefile,v 1.1.30.1.8.1 2012/03/03 06:15:13 kensmith Exp $

GROT=	../../../../../lib/libc/regex/grot

.PHONY: tests
tests:
	cd ${GROT}; make && echo "PASS regex"

.PHONY: clean
clean:
	cd ${GROT}; make spotless
