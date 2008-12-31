# $FreeBSD: src/tools/regression/lib/libc/locale/Makefile,v 1.8.18.1 2008/11/25 02:59:29 kensmith Exp $

TESTS=	test-mbrtowc	\
	test-wcrtomb	\
	test-mbsnrtowcs	\
	test-mbsrtowcs	\
	test-wcsnrtombs	\
	test-wcsrtombs	\
	test-btowc	\
	test-mbrlen	\
	test-mbtowc	\
	test-wctomb	\
	test-mbstowcs	\
	test-wcstombs	\
	test-mblen	\
	test-iswctype	\
	test-towctrans

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
