# $FreeBSD$

.include <bsd.init.mk>

TESTSDIR=	${TESTSBASE}/lib/atf/libatf-c++

ATF=		${.CURDIR:H:H:H:H}/contrib/atf
.PATH:		${ATF}/atf-c++
.PATH:		${ATF}/atf-c++/detail

CFLAGS+=	-I${ATF}

FILESDIR=	${TESTSDIR}
FILES=		macros_hpp_test.cpp
FILES+=		unused_test.cpp

# Tests in atf-c++.
.for _T in	atf_c++_test \
		build_test \
		check_test \
		config_test \
		macros_test \
		tests_test \
		utils_test
ATF_TESTS_CXX+=	${_T}
SRCS.${_T}=	${_T}.cpp test_helpers.cpp
.endfor

ATF_TESTS_SH=	pkg_config_test

# Tests in atf-c++/detail.

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
