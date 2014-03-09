# $FreeBSD$

TESTS=	test-arc4random test-fmtcheck test-fmtmsg test-fnmatch \
	test-fpclassify test-ftw test-popen test-posix_spawn test-wordexp

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}

sh-tests: test-fnmatch
	./test-fnmatch -s 1 >../../../bin/sh/builtins/case2.0
	./test-fnmatch -s 2 >../../../bin/sh/builtins/case3.0
