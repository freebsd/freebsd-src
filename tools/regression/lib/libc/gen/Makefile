# $FreeBSD: src/tools/regression/lib/libc/gen/Makefile,v 1.2 2003/02/05 16:40:18 mike Exp $

TESTS=	test-fpclassify test-wordexp

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
