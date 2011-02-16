# $FreeBSD: src/tools/regression/lib/libutil/Makefile,v 1.2.12.1 2010/12/21 17:10:29 kensmith Exp $

TESTS=	test-trimdomain test-trimdomain-nodomain test-flopen
CFLAGS+= -g -Wall -lutil

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
