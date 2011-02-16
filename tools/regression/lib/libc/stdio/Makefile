# $FreeBSD: src/tools/regression/lib/libc/stdio/Makefile,v 1.4.24.1 2010/12/21 17:10:29 kensmith Exp $

TESTS=	test-perror test-printfloat test-scanfloat
CFLAGS+= -lm

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
