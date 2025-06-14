.include <bsd.own.mk>

TESTSDIR:=	${TESTSBASE}/${RELDIR:C/libc\/tests/libc/}

# sys/ headers
FORTIFY_TCATS+=	random
FORTIFY_TCATS+=	select
FORTIFY_TCATS+=	socket
FORTIFY_TCATS+=	uio

# non-sys/ headers
FORTIFY_TCATS+=	poll
FORTIFY_TCATS+=	signal
FORTIFY_TCATS+=	stdlib
FORTIFY_TCATS+=	stdio
FORTIFY_TCATS+=	string
FORTIFY_TCATS+=	strings
FORTIFY_TCATS+=	unistd
FORTIFY_TCATS+=	wchar

# Manually run after updating the test generator.
lint-generator: .PHONY
	@if ! which luacheck>/dev/null; then \
		1>&2 echo "devel/lua-luacheck is required to regenerate and lint these tests"; \
		exit 1; \
	fi
	luacheck ${.CURDIR}/generate-fortify-tests.lua

generate-tests: .PHONY lint-generator
.for tcat in ${FORTIFY_TCATS}
ATF_TESTS_C+=	fortify_${tcat}_test

.ORDER: lint-generator generate-tests-${tcat}
generate-tests: generate-tests-${tcat}
generate-tests-${tcat}: .PHONY
	${.CURDIR}/generate-fortify-tests.lua ${tcat} > ${.CURDIR}/fortify_${tcat}_test.c
.endfor

.include <bsd.test.mk>
