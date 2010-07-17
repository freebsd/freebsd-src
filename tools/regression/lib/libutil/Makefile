# $FreeBSD: src/tools/regression/lib/libutil/Makefile,v 1.3.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

TESTS=	test-trimdomain test-trimdomain-nodomain test-flopen test-grp
CFLAGS+= -g -Wall -lutil

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
