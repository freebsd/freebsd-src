# $FreeBSD: src/tools/regression/lib/libc/regex/Makefile,v 1.1.28.1 2009/04/15 03:14:26 kensmith Exp $

GROT=	../../../../../lib/libc/regex/grot

.PHONY: tests
tests:
	cd ${GROT}; make && echo "PASS regex"

.PHONY: clean
clean:
	cd ${GROT}; make spotless
