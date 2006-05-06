# $FreeBSD: src/tools/regression/lib/libc/locale/Makefile,v 1.8 2004/11/21 03:21:22 tjr Exp $

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
