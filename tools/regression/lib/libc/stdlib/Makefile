# $FreeBSD: src/tools/regression/lib/libc/stdlib/Makefile,v 1.1.30.1.8.1 2012/03/03 06:15:13 kensmith Exp $

TESTS=	test-qsort	\
	test-heapsort	\
	test-mergesort

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
