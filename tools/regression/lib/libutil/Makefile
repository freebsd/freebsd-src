# $FreeBSD: src/tools/regression/lib/libutil/Makefile,v 1.1.2.1.8.1 2008/10/02 02:57:24 kensmith Exp $

TESTS=	test-trimdomain test-trimdomain-nodomain
CFLAGS+= -g -Wall -lutil

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
