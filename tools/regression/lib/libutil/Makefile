# $FreeBSD: src/tools/regression/lib/libutil/Makefile,v 1.3.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $

TESTS=	test-trimdomain test-trimdomain-nodomain test-flopen test-grp
CFLAGS+= -g -Wall -lutil

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
