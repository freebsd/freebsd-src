# $FreeBSD: src/tools/regression/lib/libc/stdio/Makefile,v 1.4.18.1 2008/11/25 02:59:29 kensmith Exp $

TESTS=	test-perror test-printfloat test-scanfloat
CFLAGS+= -lm

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
