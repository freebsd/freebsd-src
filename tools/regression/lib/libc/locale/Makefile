# $FreeBSD$

.PHONY: tests
tests: test-btowc test-mbrlen test-mbrtowc test-mbsrtowcs test-wcsrtombs \
		test-wcrtomb
	${.OBJDIR}/test-mbrtowc
	${.OBJDIR}/test-wcrtomb
	${.OBJDIR}/test-mbsrtowcs
	${.OBJDIR}/test-wcsrtombs
	${.OBJDIR}/test-btowc
	${.OBJDIR}/test-mbrlen

.PHONY: clean
clean:
	-rm -f test-btowc test-mbrlen test-mbrtowc test-mbsrtowcs \
		test-wcrtomb test-wcsrtombs

test-btowc: test-btowc.c

test-mbrlen: test-mbrlen.c

test-mbrtowc: test-mbrtowc.c

test-mbsrtowcs: test-mbsrtowcs.c

test-wcrtomb: test-wcrtomb.c

test-wcsrtombs: test-wcsrtombs.c
