# $FreeBSD: src/tools/regression/lib/libc/stdio/Makefile,v 1.4.22.1 2010/02/10 00:26:20 kensmith Exp $

TESTS=	test-perror test-printfloat test-scanfloat
CFLAGS+= -lm

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
