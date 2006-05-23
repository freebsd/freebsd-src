# $FreeBSD: src/tools/regression/lib/libc/regex/Makefile,v 1.1 2004/07/19 09:00:26 tjr Exp $

GROT=	../../../../../lib/libc/regex/grot

.PHONY: tests
tests:
	cd ${GROT}; make && echo "PASS regex"

.PHONY: clean
clean:
	cd ${GROT}; make spotless
