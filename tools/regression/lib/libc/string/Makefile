# $FreeBSD: src/tools/regression/lib/libc/string/Makefile,v 1.1 2002/12/18 16:59:08 mike Exp $

TESTS=	test-strerror

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
