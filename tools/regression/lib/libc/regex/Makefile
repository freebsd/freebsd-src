# $FreeBSD: src/tools/regression/lib/libc/regex/Makefile,v 1.1.30.1.2.1 2009/10/25 01:10:29 kensmith Exp $

GROT=	../../../../../lib/libc/regex/grot

.PHONY: tests
tests:
	cd ${GROT}; make && echo "PASS regex"

.PHONY: clean
clean:
	cd ${GROT}; make spotless
