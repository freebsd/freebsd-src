# $FreeBSD$

TESTS=	test-fmemopen test-getdelim test-mkostemp test-open_memstream \
	test-open_wmemstream test-perror test-print-positional test-printbasic \
	test-printfloat test-scanfloat 
CFLAGS+= -lm

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
