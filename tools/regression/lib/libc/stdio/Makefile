# $FreeBSD: src/tools/regression/lib/libc/stdio/Makefile,v 1.3 2003/06/28 09:03:44 das Exp $

TESTS=	test-perror test-printfloat test-scanfloat

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
