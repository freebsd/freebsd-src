# $FreeBSD$

.include <bsd.own.mk>

SUBDIR= \
	bcd \
	caesar \
	factor \
	fortune \
	grdc \
	morse \
	number \
	pom \
	ppt \
	primes \
	random \
	${_tests}

.if ${MK_TESTS} != "no"
_tests=	tests
.endif

.include <bsd.subdir.mk>
