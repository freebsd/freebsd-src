# $FreeBSD: src/tools/regression/lib/libc/stdlib/Makefile,v 1.1 2004/06/25 12:31:12 sobomax Exp $

TESTS=	test-qsort	\
	test-heapsort	\
	test-mergesort

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
