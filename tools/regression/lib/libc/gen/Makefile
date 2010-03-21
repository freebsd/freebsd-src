# $FreeBSD: src/tools/regression/lib/libc/gen/Makefile,v 1.2.34.1 2010/02/10 00:26:20 kensmith Exp $

TESTS=	test-fpclassify test-wordexp

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
