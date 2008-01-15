# $FreeBSD: src/tools/regression/lib/libutil/Makefile,v 1.1.2.1 2005/12/22 03:47:05 brooks Exp $

TESTS=	test-trimdomain test-trimdomain-nodomain
CFLAGS+= -g -Wall -lutil

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
