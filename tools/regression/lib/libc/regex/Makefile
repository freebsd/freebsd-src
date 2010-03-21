# $FreeBSD: src/tools/regression/lib/libc/regex/Makefile,v 1.1.32.1 2010/02/10 00:26:20 kensmith Exp $

GROT=	../../../../../lib/libc/regex/grot

.PHONY: tests
tests:
	cd ${GROT}; make && echo "PASS regex"

.PHONY: clean
clean:
	cd ${GROT}; make spotless
