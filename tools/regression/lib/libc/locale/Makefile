# $FreeBSD$

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
	test-towctrans	\
	test-sgetrune	\
	test-sputrune

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
