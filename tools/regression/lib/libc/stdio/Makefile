# $FreeBSD$

TESTS=	test-perror test-printfloat test-scanfloat

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
