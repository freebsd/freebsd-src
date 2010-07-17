# $FreeBSD: src/tools/regression/lib/libc/regex/Makefile,v 1.1.30.1.4.1 2010/06/14 02:09:06 kensmith Exp $

GROT=	../../../../../lib/libc/regex/grot

.PHONY: tests
tests:
	cd ${GROT}; make && echo "PASS regex"

.PHONY: clean
clean:
	cd ${GROT}; make spotless
