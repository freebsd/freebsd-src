# $FreeBSD: src/tools/regression/lib/libc/stdio/Makefile,v 1.7.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

TESTS=	test-getdelim test-perror test-print-positional test-printbasic test-printfloat test-scanfloat
CFLAGS+= -lm

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
