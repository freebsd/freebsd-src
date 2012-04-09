# $FreeBSD: src/tools/regression/lib/libc/gen/Makefile,v 1.3.2.3.6.1 2012/03/03 06:15:13 kensmith Exp $

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
