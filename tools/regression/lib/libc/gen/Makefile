# $FreeBSD: src/tools/regression/lib/libc/gen/Makefile,v 1.2.30.1 2008/11/25 02:59:29 kensmith Exp $

TESTS=	test-fpclassify test-wordexp

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
