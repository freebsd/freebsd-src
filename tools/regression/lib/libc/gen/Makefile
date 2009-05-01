# $FreeBSD: src/tools/regression/lib/libc/gen/Makefile,v 1.2.32.1 2009/04/15 03:14:26 kensmith Exp $

TESTS=	test-fpclassify test-wordexp

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
