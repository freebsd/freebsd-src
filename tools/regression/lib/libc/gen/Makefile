# $FreeBSD: src/tools/regression/lib/libc/gen/Makefile,v 1.2.36.1 2010/12/21 17:10:29 kensmith Exp $

TESTS=	test-fpclassify test-wordexp

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
