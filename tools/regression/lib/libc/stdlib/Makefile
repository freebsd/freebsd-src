# $FreeBSD: src/tools/regression/lib/libc/stdlib/Makefile,v 1.1.26.1 2008/11/25 02:59:29 kensmith Exp $

TESTS=	test-qsort	\
	test-heapsort	\
	test-mergesort

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
