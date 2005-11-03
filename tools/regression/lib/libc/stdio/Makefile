# $FreeBSD: src/tools/regression/lib/libc/stdio/Makefile,v 1.4 2005/03/01 01:43:05 das Exp $

TESTS=	test-perror test-printfloat test-scanfloat
CFLAGS+= -lm

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
