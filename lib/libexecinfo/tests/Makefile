# $FreeBSD$

PACKAGE=	tests

TESTSRC=	${SRCTOP}/contrib/netbsd-tests/lib/libexecinfo

.include <bsd.own.mk>

# Perhaps not surprisingly, the backtrace symbol test needs symbols.  Leave
# them in:
STRIP=
# It could probably be made to work without symbols by checking pointer values
# instead, but part of the tested functionality is the symbol resolution logic,
# so that doesn't seem useful.
NETBSD_ATF_TESTS_C+=	backtrace_test

LIBADD.backtrace_test+=	execinfo

.include <netbsd-tests.test.mk>

.include <bsd.test.mk>
