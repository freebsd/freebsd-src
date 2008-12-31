# $FreeBSD: src/tools/regression/lib/libc/regex/Makefile,v 1.1.26.1 2008/11/25 02:59:29 kensmith Exp $

GROT=	../../../../../lib/libc/regex/grot

.PHONY: tests
tests:
	cd ${GROT}; make && echo "PASS regex"

.PHONY: clean
clean:
	cd ${GROT}; make spotless
