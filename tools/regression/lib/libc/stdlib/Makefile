# $FreeBSD: src/tools/regression/lib/libc/stdlib/Makefile,v 1.1.24.1 2008/10/02 02:57:24 kensmith Exp $

TESTS=	test-qsort	\
	test-heapsort	\
	test-mergesort

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
