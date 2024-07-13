.include <bsd.own.mk>

TESTSDIR:=	${TESTSBASE}/${RELDIR:C/libc\/tests/libc/}

FORTIFY_TCATS+=	stdio
FORTIFY_TCATS+=	string
FORTIFY_TCATS+=	strings
FORTIFY_TCATS+=	unistd

# Manually run after updating the test generator.
generate-tests: .PHONY
.for tcat in ${FORTIFY_TCATS}
ATF_TESTS_C+=	fortify_${tcat}_test

generate-tests: generate-tests-${tcat}
generate-tests-${tcat}: .PHONY
	${.CURDIR}/generate-fortify-tests.lua ${tcat} > ${.CURDIR}/fortify_${tcat}_test.c
.endfor

.include <bsd.test.mk>
