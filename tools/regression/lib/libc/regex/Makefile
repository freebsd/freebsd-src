# $FreeBSD: src/tools/regression/lib/libc/regex/Makefile,v 1.1.24.1 2008/10/02 02:57:24 kensmith Exp $

GROT=	../../../../../lib/libc/regex/grot

.PHONY: tests
tests:
	cd ${GROT}; make && echo "PASS regex"

.PHONY: clean
clean:
	cd ${GROT}; make spotless
