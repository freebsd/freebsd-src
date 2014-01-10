# $FreeBSD$

.include <bsd.init.mk>

TESTSDIR=	${TESTSBASE}/lib/atf/libatf-c++
TESTS_SUBDIRS=	detail

ATF=		${.CURDIR:H:H:H:H}/contrib/atf
.PATH:		${ATF}/atf-c++
.PATH:		${ATF}/atf-c++/detail

CFLAGS+=	-I${ATF}

FILESDIR=	${TESTSDIR}
FILES=		macros_hpp_test.cpp
FILES+=		unused_test.cpp

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

.include <atf.test.mk>
