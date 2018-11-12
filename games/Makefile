# $FreeBSD$

.include <src.opts.mk>

SUBDIR= \
	caesar \
	factor \
	fortune \
	grdc \
	morse \
	number \
	pom \
	primes \
	random \
	${_tests}

.if ${MK_TESTS} != "no"
_tests=	tests
.endif

.include <bsd.subdir.mk>
