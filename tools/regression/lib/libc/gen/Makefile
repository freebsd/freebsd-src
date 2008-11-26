# $FreeBSD: src/tools/regression/lib/libc/gen/Makefile,v 1.2.28.1 2008/10/02 02:57:24 kensmith Exp $

TESTS=	test-fpclassify test-wordexp

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
