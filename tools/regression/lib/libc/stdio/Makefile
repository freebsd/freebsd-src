# $FreeBSD: src/tools/regression/lib/libc/stdio/Makefile,v 1.2 2003/04/05 22:13:01 das Exp $

TESTS=	test-perror test-printfloat

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
