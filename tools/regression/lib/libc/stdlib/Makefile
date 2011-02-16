# $FreeBSD: src/tools/regression/lib/libc/stdlib/Makefile,v 1.1.34.1 2010/12/21 17:10:29 kensmith Exp $

TESTS=	test-qsort	\
	test-heapsort	\
	test-mergesort

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
