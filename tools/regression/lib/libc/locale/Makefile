# $FreeBSD: src/tools/regression/lib/libc/locale/Makefile,v 1.4 2002/11/09 04:33:02 tjr Exp $

TESTS=	test-mbrtowc	\
	test-wcrtomb	\
	test-mbsrtowcs	\
	test-wcsrtombs	\
	test-btowc	\
	test-mbrlen	\
	test-mbtowc	\
	test-wctomb	\
	test-mbstowcs	\
	test-wcstombs	\
	test-mblen

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
