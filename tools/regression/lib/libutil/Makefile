# $FreeBSD: src/tools/regression/lib/libutil/Makefile,v 1.3.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

TESTS=	test-trimdomain test-trimdomain-nodomain test-flopen test-grp
CFLAGS+= -g -Wall -lutil

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
