# $FreeBSD$

.include <bsd.init.mk>

TESTSDIR=	${TESTSBASE}/lib/atf/libatf-c++/detail

ATF=		${.CURDIR:H:H:H:H:H}/contrib/atf
.PATH:		${ATF}/atf-c++/detail

CFLAGS+=	-I${ATF}

.for _T in	application_test \
		env_test \
		exceptions_test \
		expand_test \
		fs_test \
		parser_test \
		process_test \
		sanity_test \
		text_test \
		ui_test
ATF_TESTS_CXX+=	${_T}
SRCS.${_T}=	${_T}.cpp test_helpers.cpp
.endfor

.include <atf.test.mk>
