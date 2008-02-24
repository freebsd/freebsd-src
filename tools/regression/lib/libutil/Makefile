# $FreeBSD: src/tools/regression/lib/libutil/Makefile,v 1.2 2007/08/03 11:29:49 des Exp $

TESTS=	test-trimdomain test-trimdomain-nodomain test-flopen
CFLAGS+= -g -Wall -lutil

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
