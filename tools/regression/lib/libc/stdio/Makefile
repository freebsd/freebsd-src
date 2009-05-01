# $FreeBSD: src/tools/regression/lib/libc/stdio/Makefile,v 1.4.20.1 2009/04/15 03:14:26 kensmith Exp $

TESTS=	test-perror test-printfloat test-scanfloat
CFLAGS+= -lm

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
