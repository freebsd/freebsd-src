# $FreeBSD: src/tools/regression/lib/libc/stdio/Makefile,v 1.4.16.1 2008/10/02 02:57:24 kensmith Exp $

TESTS=	test-perror test-printfloat test-scanfloat
CFLAGS+= -lm

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
