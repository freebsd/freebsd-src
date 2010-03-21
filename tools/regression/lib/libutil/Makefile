# $FreeBSD: src/tools/regression/lib/libutil/Makefile,v 1.2.10.1 2010/02/10 00:26:20 kensmith Exp $

TESTS=	test-trimdomain test-trimdomain-nodomain test-flopen
CFLAGS+= -g -Wall -lutil

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
