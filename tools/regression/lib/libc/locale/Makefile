# $FreeBSD$

.PHONY: tests
tests: test-mbrtowc test-wcrtomb
	${.OBJDIR}/test-mbrtowc
	${.OBJDIR}/test-wcrtomb

.PHONY: clean
clean:
	-rm -f test-mbrtowc test-wcrtomb

test-mbrtowc: test-mbrtowc.c

test-wcrtomb: test-wcrtomb.c
