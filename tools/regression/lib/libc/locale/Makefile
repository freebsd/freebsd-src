# $FreeBSD$

TESTS=	test-mbrtowc	\
	test-wcrtomb	\
	test-mbsrtowcs	\
	test-wcsrtombs	\
	test-btowc	\
	test-mbrlen

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
