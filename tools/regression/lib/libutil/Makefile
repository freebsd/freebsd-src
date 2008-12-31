# $FreeBSD: src/tools/regression/lib/libutil/Makefile,v 1.2.6.1 2008/11/25 02:59:29 kensmith Exp $

TESTS=	test-trimdomain test-trimdomain-nodomain test-flopen
CFLAGS+= -g -Wall -lutil

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
