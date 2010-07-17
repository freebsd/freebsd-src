# $FreeBSD: src/tools/regression/lib/libc/gen/Makefile,v 1.3.2.3.2.1 2010/06/14 02:09:06 kensmith Exp $

TESTS=	test-fmtcheck test-fnmatch test-fpclassify test-wordexp

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}

sh-tests: test-fnmatch
	./test-fnmatch -s 1 >../../../bin/sh/builtins/case2.0
	./test-fnmatch -s 2 >../../../bin/sh/builtins/case3.0
