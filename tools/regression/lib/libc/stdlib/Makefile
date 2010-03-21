# $FreeBSD: src/tools/regression/lib/libc/stdlib/Makefile,v 1.1.32.1 2010/02/10 00:26:20 kensmith Exp $

TESTS=	test-qsort	\
	test-heapsort	\
	test-mergesort

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
